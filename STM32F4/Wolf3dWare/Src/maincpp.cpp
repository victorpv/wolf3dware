#include "Firmware/Kernel.h"
#include "Firmware/GCodeProcessor.h"
#include "Firmware/Dispatcher.h"
#include "Firmware/GCode.h"
#include "Firmware/MotionControl.h"
#include "Firmware/Block.h"
#include "Firmware/Planner.h"
#include "Firmware/Actuator.h"

#include <map>
#include <vector>
#include <iostream>
#include <stdint.h>
#include <ctype.h>
#include <cmath>
#include <assert.h>
#include <malloc.h>
#include <string.h>

using namespace std;

extern "C" int maincpp()
{
#if 0
	//malloc_stats();
	GCodeProcessor& gp= THEKERNEL.getGCodeProcessor();

	// Parse gcode - This would come from Serial port
	GCodeProcessor::GCodes_t gcodes= gp.parse("G1 X100 Y0 E1.0 F6000 G1 X100 Y100 E2.0 G1 X0 Y100 E3.0 G1 X0 Y0 E4.0 G1 X100 Y50 E4.75 M114");

	// dispatch gcode to MotionControl and Planner
	for(auto i : gcodes) {
		if(THEDISPATCHER.dispatch(i)) {
			// TODO send the result to the place it came from
			std::cout << THEDISPATCHER.getResult();

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
		//if(!block.ready()) wait longer
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
	return 0;
}

extern "C" bool serial_reply(const char*, size_t);

bool handleCommand(const char *line)
{
	bool handled= true;
	std::ostringstream oss;

	if(strcmp(line, "dump") == 0) {
		// dump planned block queue
		THEKERNEL.getPlanner().dump(oss);

	}else if(strcmp(line, "version") == 0) {
		oss << "Wolf3dWare V0.1\n";

	}else{
		oss << "Unknown command: " << line << "\n";
		handled= false;
	}

	std::string str= oss.str();
	if(!str.empty()) {
		serial_reply(str.c_str(), str.size());
	}

	return handled;
}


// gets called for each received line from USB serial port
extern "C" bool commandLineHandler(const char *line)
{
	if(*line == 24) {
		// ^X means this is a command not gcode
		handleCommand(&line[1]);
		return true;
	}

	// Handle Gcode
	GCodeProcessor& gp= THEKERNEL.getGCodeProcessor();

	// Parse gcode
	GCodeProcessor::GCodes_t gcodes= gp.parse(line);

	// dispatch gcode to MotionControl and Planner
	for(auto i : gcodes) {
		if(THEDISPATCHER.dispatch(i)) {
			// send the result to the place it came from
			serial_reply(THEDISPATCHER.getResult().c_str(), THEDISPATCHER.getResult().size());

		}else{
			// no handler for this gcode, return ok - nohandler
			const char *str= "ok - nohandler\n";
			serial_reply(str, strlen(str));
		}
	}
	return true;
}
