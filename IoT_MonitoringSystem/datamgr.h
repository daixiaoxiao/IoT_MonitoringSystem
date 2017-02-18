#ifndef DATAMGR_H
#define DATAMGR_H

#include <time.h>
#include <stdio.h>
#include "config.h"
#include "sbuffer.h"
#include "lib/dplist.h"

#ifndef RUN_AVG_LENGTH
  #define RUN_AVG_LENGTH 5
#endif

#ifndef SET_MAX_TEMP
  #error SET_MAX_TEMP not set
#endif

#ifndef SET_MIN_TEMP
  #error SET_MIN_TEMP not set
#endif

#define NUM_SENSORS 8

/*
 * Reads continiously all data from the shared buffer data structure, parse the room_id's
 * and calculate the running avarage for all sensor ids
 * When *buffer becomes NULL the method finishes. This method will NOT automatically free all used memory
 */
void datamgr_parse_sensor_data(FILE * fp_sensor_map, sbuffer_t ** buffer);

/*
 * This method should be called to clean up the datamgr, and to free all used memory. 
 * After this, any call to datamgr_get_room_id, datamgr_get_avg, datamgr_get_last_modified 
 * or datamgr_get_total_sensors will not return a valid result
 */
void datamgr_free();

/*
 * Gets the room ID for a certain sensor ID
 */
uint16_t datamgr_get_room_id(sensor_id_t sensor_id);

/*
 * Gets the running AVG of a certain senor ID 
 */
sensor_value_t datamgr_get_avg(sensor_id_t sensor_id);

/*
 * Returns the time of the last reading for a certain sensor ID
 */
time_t datamgr_get_last_modified(sensor_id_t sensor_id);

/*
 * Return the total amount of unique sensor ID's recorded by the datamgr
 */
int datamgr_get_total_sensors();

#endif /* DATAMGR_H */

