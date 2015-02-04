// gcode processor and dispatcher

#include "GCodeProcessor.h"
#include "GCode.h"

#include <map>
#include <vector>
#include <stdint.h>
#include <ctype.h>
#include <cmath>

using namespace std;

static tuple<uint16_t, uint16_t, float> parse_code(const char *&p)
{
	int a= 0, b= 0;
	float f= strtof(p, nullptr);
	while(*p && isdigit(*p)) {
		a = (a*10) + (*p-'0');
		++p;
	}
	if(*p == '.') {
		++p;
		while(*p && isdigit(*p)) {
			b = (b*10) + (*p-'0');
			++p;
		}
	}
	return make_tuple(a, b, f);
}

GCodeProcessor::GCodeProcessor(){}

GCodeProcessor::~GCodeProcessor() {}

// Parse the line containing 1 or more gcode words
GCodeProcessor::GCodes_t GCodeProcessor::parse(const char *line)
{
	GCodes_t gcodes;
	GCode gc;
	bool start= true;
	const char *p= line;
	while(*p != '\0') {
		if(isspace(*p)){
			++p;
			continue;
		}
		if(*p == ';') break; // skip rest of line for comment
		if(*p == '(') {
			// skip comments in ( ... )
			while(*p != '\0' && *p != ')') ++p;
			if(*p) ++p;
			continue;
		}

		char c= toupper(*p++);
		if((c == 'G' || c == 'M') && !start) {
			gcodes.push_back(gc);
			gc.clear();
			start= true;
		}

		// extract gcode word G01{.123}
		tuple<uint16_t, uint16_t, float> code= parse_code(p);
		if(start) {
			if(c == 'G' || c == 'M'){
				gc.set_command(c, get<0>(code), get<1>(code));
				if(c == 'G' && get<0>(code) <= 3) {
					group1= gc;
				}

			}else{
				gc.set_command('G', group1.get_code(), group1.get_subcode()); // modal group1, copies G code and subcode for this line
				gc.add_arg(c, get<2>(code));
			}

			start= false;

		}else{
			gc.add_arg(c, get<2>(code));
		}
	}
	gcodes.push_back(gc);
	return gcodes;
}
