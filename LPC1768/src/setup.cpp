#include "config.h"

#include "Firmware/Kernel.h"
#include "Firmware/GCodeProcessor.h"
#include "Firmware/Dispatcher.h"
#include "Firmware/GCode.h"
#include "Firmware/MotionControl.h"
#include "Firmware/Block.h"
#include "Firmware/Planner.h"
#include "Firmware/Actuator.h"

#include "mbed.h"
#include "rtos.h"

#include "Lock.h"

#include "mri.h"

#include <algorithm>

// externals
extern void initControl();
extern uint16_t* getADC(uint8_t ch);
extern void InitializeADC(int sample_rate);
extern void startADC();

#ifdef PRINTER3D
#include "Firmware/Tools/TemperatureControl.h"
#include "Firmware/Tools/Thermistor.h"
#include "Firmware/Tools/Extruder.h"
#endif


// local

//static uint16_t xendstop, yendstop, zendstop;


// define specific pins, set 3rd parameter to true if inverted

#ifdef SMOOTHIEBOARD
static DigitalOut X_StepPin(P2_0);
static DigitalOut X_DirPin (P0_5);
static DigitalOut X_EnbPin (P0_4);
static DigitalOut Y_StepPin(P2_1);
static DigitalOut Y_DirPin (P0_11);
static DigitalOut Y_EnbPin (P0_10);
static DigitalOut Z_StepPin(P2_2);
static DigitalOut Z_DirPin (P0_20);
static DigitalOut Z_EnbPin (P0_19);
static DigitalOut E_StepPin(P2_3);
static DigitalOut E_DirPin (P0_22);
static DigitalOut E_EnbPin (P0_21);

static DigitalOut LED1Pin  (P1_18);
static DigitalOut LED2Pin  (P1_19);
static DigitalOut LED3Pin  (P1_20);
static DigitalOut LED4Pin  (P1_21);
static DigitalOut PLAY_LED (P4_28);

static DigitalIn ButtonPin  (P2_12);
static DigitalIn XEndstopPin(P1_24);
static DigitalIn YEndstopPin(P1_26);
static DigitalIn ZEndstopPin(P1_28);

static DigitalOut TriggerPin(P1_22);

static PwmOut HotendHeater(P2_5);
static PwmOut BedHeater(P1_23); // #rd large mosfet on 5 driver

#elif AZTEEGX5_MINI

#else

#error "need to define the board"

#endif

static void initializePins()
{
	// set pins to output, and initially low
 	X_StepPin= 0;
 	X_DirPin= 0;
 	X_EnbPin= 0;
 	Y_StepPin= 0;
 	Y_DirPin= 0;
 	Y_EnbPin= 0;
 	Z_StepPin= 0;
 	Z_DirPin= 0;
 	Z_EnbPin= 0;
 	E_StepPin= 0;
 	E_DirPin= 0;
 	E_EnbPin= 0;

 	// no pullup, IRQ, rising, low priority
 	ButtonPin.mode(PullNone);

 	// endstops IRQ on rising edge, Normally Closed to Ground
//  	xendstop= XEndstopPin::input(true, true, true);
//  	yendstop= YEndstopPin::input(true, true, true);
//  	zendstop= ZEndstopPin::input(true, true, true);

 	TriggerPin= 0;

	LED1Pin= 0;
	LED2Pin= 0;
	LED3Pin= 0;
	LED4Pin= 0;
	PLAY_LED= 1;
}

#if 0
void getPosition(float *x, float *y, float *z, float *e)
{
	*x= THEKERNEL.getMotionControl().getActuator('X').getCurrentPositionInmm();
	*y= THEKERNEL.getMotionControl().getActuator('Y').getCurrentPositionInmm();
	if(z != NULL) *z= THEKERNEL.getMotionControl().getActuator('Z').getCurrentPositionInmm();
	if(e != NULL) *e= THEKERNEL.getMotionControl().getActuator('E').getCurrentPositionInmm();
}
#endif

static size_t doDelay(void *, size_t, uint32_t ms)
{
	Thread::wait(ms);
	return 0;
}

void setLed(int led, bool on)
{
	switch(led) {
		case 1: LED1Pin= on?1:0; break;
		case 2: LED2Pin= on?1:0; break;
		case 3: LED3Pin= on?1:0; break;
		case 4: LED4Pin= on?1:0; break;
		case 5: PLAY_LED= on?0:1; break;
		case 10: TriggerPin= on?1:0; break;
	}
}

#ifdef PRINTER3D
static uint16_t last_adc[2][4]{{0, 0, 0, 0}, {0, 0, 0, 0}};
// to oversample the ADC to get 4 extra bits (16bits from 12bit ADC) you need to sample 4^4 = 256 samples,
// sum them then shift right 4 bits to get the 16bit result
// we eliminate the top and bottom 128 after sorting to get rid of spikes and some noise
// sampling @ 40Khz takes 23602uS to complete the sample for two channels
static uint16_t sampleADC(int channel, bool verbose= false)
{
	int samples= OVERSAMPLE_SAMPLES; // the number of samples required
	uint32_t acc= 0;
	uint16_t *adc_buf= getADC(channel);
	// sort the buffer
	std::sort(adc_buf, adc_buf+samples);
	if(verbose) {
		for (int i = 0; i < samples; ++i) {
			printf("%u, ", adc_buf[i]);
			if((i % 32) == 0) printf("\n");
		}
		printf("\n");
	}
	// eliminate the top and bottom 128 (OVERSAMPLE_SAMPLES/4)
	for (int i = 128; i < samples-128; ++i) {
		// accumulate the samples
		acc += adc_buf[i];
	}

	return acc >> OVERSAMPLE_ADC; // return the 16bit result
}

static RtosTimer *adcReadTimer;
static void readADCTimer(void const *)
{
	// handle a running average, each read goes into the queue pushing the oldest out
	last_adc[0][3]= last_adc[0][2];
	last_adc[0][2]= last_adc[0][1];
	last_adc[0][1]= last_adc[0][0];
	last_adc[0][0]= sampleADC(0);

	last_adc[1][3]= last_adc[1][2];
	last_adc[1][2]= last_adc[1][1];
	last_adc[1][1]= last_adc[1][0];
	last_adc[1][0]= sampleADC(1);
	getADC(255); // start DMA for next read
}

// take the running average of the last 4 samples
uint16_t getLastADC(uint8_t ch)
{
	return ((uint32_t)last_adc[ch][0]+last_adc[ch][1]+last_adc[ch][2]+last_adc[ch][3])/4;
}

void InitializePWM()
{
 	HotendHeater.period_us(1000); // 1KHz period
 	HotendHeater= 0;
 	HotendHeater.period_us(1000);
 	BedHeater= 0;
}

void setPWM(uint8_t channel, float percent)
{
	switch(channel) {
		case 0: HotendHeater= percent/100.0F; break;
		case 1: BedHeater= percent/100.0F; break;
	}
}
#endif

// TODO use sdcard
static size_t writeFlash(void *, size_t, uint32_t)
{
	return 0;
}

static size_t readFlash(void *, size_t, uint32_t)
{
	return 0;
}

int setup()
{
	// creates Kernel singleton and other singletons and Initializes MotionControl
	MotionControl& mc= THEKERNEL.getMotionControl();

	// assign the HAL function for Non volatile memory in the Kernel
	//THEKERNEL.assignHALFunction(Kernel::NV_INIT, [](void*,size_t)  { HAL_FLASH_INIT(); });
	THEKERNEL.assignHALFunction(Kernel::NV_WRITE, writeFlash);
	THEKERNEL.assignHALFunction(Kernel::NV_READ, readFlash);

	// also a HAL independent task delay
	THEKERNEL.assignHALFunction(Kernel::DELAY, doDelay);

	// initialize Kernel and its modules
	THEKERNEL.initialize();

	// setup the pins
	initializePins();

	// Setup pins for each Actuator
	mc.getActuator('X').assignHALFunction(Actuator::SET_STEP, [](bool on)  { X_StepPin= on; });
	mc.getActuator('X').assignHALFunction(Actuator::SET_DIR, [](bool on)   { X_DirPin= on; });
	mc.getActuator('X').assignHALFunction(Actuator::SET_ENABLE, [](bool on){ X_EnbPin= on; });
	mc.getActuator('Y').assignHALFunction(Actuator::SET_STEP, [](bool on)  { Y_StepPin= on; });
	mc.getActuator('Y').assignHALFunction(Actuator::SET_DIR, [](bool on)   { Y_DirPin= on; });
	mc.getActuator('Y').assignHALFunction(Actuator::SET_ENABLE, [](bool on){ Y_EnbPin= on; });
	mc.getActuator('Z').assignHALFunction(Actuator::SET_STEP, [](bool on)  { Z_StepPin= on; });
	mc.getActuator('Z').assignHALFunction(Actuator::SET_DIR, [](bool on)   { Z_DirPin= on; });
	mc.getActuator('Z').assignHALFunction(Actuator::SET_ENABLE, [](bool on){ Z_EnbPin= on; });
	mc.getActuator('E').assignHALFunction(Actuator::SET_STEP, [](bool on)  { E_StepPin= on; });
	mc.getActuator('E').assignHALFunction(Actuator::SET_DIR, [](bool on)   { E_DirPin= on; });
	mc.getActuator('E').assignHALFunction(Actuator::SET_ENABLE, [](bool on){ E_EnbPin= on;  });


#ifdef PRINTER3D
	// needed for hotend
	InitializePWM(); // PWM control
	InitializeADC(OVERSAMPLE_SAMPLERATE); // ADC control
	Thread::wait(50);

	// timer that reads the ADC at a regular interval
	adcReadTimer= new RtosTimer(readADCTimer, osTimerPeriodic, (void *)0);
	adcReadTimer->start(1000/25); // 25Hz slightly faster than temperature control

	// Setup the Temperature Control and sensors
	static Thermistor thermistor0(0, 4095*16);
	thermistor0.assignHALFunction(Thermistor::GET_ADC, getLastADC);

	static Thermistor thermistor1(1, 4095*16);
	thermistor1.assignHALFunction(Thermistor::GET_ADC, getLastADC);

	static TemperatureControl tc("T", 0, thermistor0);
	tc.assignHALFunction(TemperatureControl::SET_PWM, setPWM);
	tc.initialize();

	static TemperatureControl bc("B", 1, thermistor1);
	bc.assignHALFunction(TemperatureControl::SET_PWM, setPWM);
	bc.initialize();

	// use same index as the associated temperature control
	// specify which axis is the extruder
	static Extruder ex('E', 0);
	ex.initialize();

#endif

	initControl();

	// load configuration from non volatile storage
	//THEDISPATCHER.loadConfiguration(); // same as M501
	//sendReply(THEDISPATCHER.getResult());

	return 0;
}

void tests()
{
#if 0
	//malloc_stats();
	GCodeProcessor& gp= THEKERNEL.getGCodeProcessor();

	// Parse gcode - This would come from Serial port
	GCodeProcessor::GCodes_t gcodes;
	gp.parse("G1 X100 Y0 E1.0 F6000 G1 X100 Y100 E2.0 G1 X0 Y100 E3.0 G1 X0 Y0 E4.0 G1 X100 Y50 E4.75 M114", gcodes);

	// dispatch gcode to MotionControl and Planner
	for(auto& i : gcodes) {
		std::string ret= THEDISPATCHER.dispatch(i);
		if(!ret.empty()) {
			// TODO send the result to the place it came from
			std::cout << ret;

		}else{
			// no handler for this gcode, return ok - nohandler
		}
	}

	// dump planned block queue
	THEKERNEL.getPlanner().dump(cout);

	//malloc_stats();

	const Actuator& xact= THEKERNEL.getMotionControl().getActuator('X');
	const Actuator& yact= THEKERNEL.getMotionControl().getActuator('Y');
	const Actuator& eact= THEKERNEL.getMotionControl().getActuator('E');
	const float xpos[]{100,100,0,0,100};
	const float ypos[]{0,100,100,0,50};
	const float epos[]{1,2,3,4,4.75F};

	int cnt= 0;

	// iterate over block queue and setup steppers
	Planner::Queue_t& q= THEKERNEL.getPlanner().getQueue();
	while(!q.empty()) {
		Block block= q.back();
		q.pop_back();
		std::cout << "Playing Block: " << block.id << "\n";
		THEKERNEL.getMotionControl().issueMove(block);

		// simulate step ticker
		uint32_t current_tick= 0;
		bool r= true;
		while(r) {
			++current_tick;
			// sends ticks to all actuators, returns false when all have finished all steps
			r= THEKERNEL.getMotionControl().issueTicks(current_tick);
		}

		// check we got where we requested to go
		if(xact.getCurrentPositionInmm() != xpos[cnt]){ std::cout << "Error " << xact.getCurrentPositionInmm() << " != " << xpos[cnt] << "\n"; }
		if(yact.getCurrentPositionInmm() != ypos[cnt]){ std::cout << "Error " << yact.getCurrentPositionInmm() << " != " << ypos[cnt] << "\n"; }
		if(eact.getCurrentPositionInmm() != epos[cnt]){ std::cout << "Error " << eact.getCurrentPositionInmm() << " != " << epos[cnt] << "\n"; }

		++cnt;
		std::cout << "Done\n";
	}

	//malloc_stats();
#endif
}
