// k4abt_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <thread>
#include <k4abt.h>
#include <k4a/k4a.h>
#include <k4abttypes.h>
using namespace std;

void get_imu_data(k4a_device_t kinect)
{
	int32_t timeout_ms = 1000;

	//capture IMU data
	k4a_imu_sample_t imu_data;
	
	while (true)
	{
		
		k4a_device_get_imu_sample(kinect, &imu_data, timeout_ms);
		cout << "x acceleration: " << imu_data.acc_sample.xyz.x << "\n";   //measured in m/s^2
		cout << "y acceleration: " << imu_data.acc_sample.xyz.y << "\n";
		cout << "z acceleration: " << imu_data.acc_sample.xyz.z << "\n";
		cout << "acceleration timestamp: " << imu_data.acc_timestamp_usec << "\n";   //time measured in microseconds 
		cout << "x angular velocity: " << imu_data.gyro_sample.xyz.x << "\n";   //gyroscopes measure angular velocity in rad/s
		cout << "y angular velocity: " << imu_data.gyro_sample.xyz.y << "\n";
		cout << "z angular velocity: " << imu_data.gyro_sample.xyz.z << "\n";
		cout << "angular velocity timestamp: " << imu_data.gyro_timestamp_usec << "\n";

		static uint64_t last_acc_timestamp, last_gyro_timestamp;
		if (last_acc_timestamp != 0) {
			cout << "acceleration timestamp diff: " << imu_data.acc_timestamp_usec - last_acc_timestamp << "\n";   //time measured in microseconds 
		}
		last_acc_timestamp = imu_data.acc_timestamp_usec;



	}
}


int main()
{
	//opens the kinect device
	k4a_device_t kinect = NULL;
	k4a_device_open(K4A_DEVICE_DEFAULT, &kinect);  

	//configure camera here
	k4a_device_configuration_t deviceConfig = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	deviceConfig.camera_fps = K4A_FRAMES_PER_SECOND_15;
	deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_720P;
	deviceConfig.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
	
	//start camera. if can't, prints back error
	if (K4A_FAILED(k4a_device_start_cameras(kinect, &deviceConfig)))
	{
		cout << "Failed to start camera\n";
		k4a_device_close(kinect);
		return 1;
	}

	//calibrate camera
	k4a_calibration_t sensor_calibration;
	k4a_device_get_calibration(kinect, K4A_DEPTH_MODE_NFOV_UNBINNED, K4A_COLOR_RESOLUTION_720P, &sensor_calibration);

	//create a body tracker 
	k4abt_tracker_t body_tracker = NULL;
	if (K4A_FAILED(k4abt_tracker_create(&sensor_calibration, &body_tracker)))
	{
		cout << "Failed to create a body tracker\n";
		k4a_device_close(kinect);
		return 1;
	}
	
	//start IMU sensor
	if (K4A_FAILED(k4a_device_start_imu(kinect)))
	{
		cout << "Failed to start IMU sensor\n";
	}

	thread doThis(get_imu_data, kinect);
	doThis.detach();

	int counter_no_body = 0;
	while (counter_no_body < 100)   //if the camera does not detect a body after 100 times, the program will close tracker & stop camera
	{

		//captures object. stores in capture_handle  
		k4a_capture_t capture_handle; 
		int32_t timeout_ms = 1000;
		k4a_device_get_capture(kinect, &capture_handle, timeout_ms);
		//TODO depth image timestamp, compare to accel & gyro timestamp 

		//put capture data in line to process
		k4a_wait_result_t queue_result = k4abt_tracker_enqueue_capture(body_tracker, capture_handle, timeout_ms);
		k4a_capture_release(capture_handle); 
		if (queue_result == K4A_WAIT_RESULT_FAILED)
		{
			cout << "Capture data failed to be placed in queue to process\n"; 
		}

		//releases data that is stored in body_frame. processes body_frame data
		k4abt_frame_t body_frame = NULL;
		k4a_wait_result_t pop_result = k4abt_tracker_pop_result(body_tracker, &body_frame, timeout_ms);
		if (pop_result == K4A_WAIT_RESULT_SUCCEEDED)
		{
			//because successful, start processing body_frame data

			size_t num_bodies = k4abt_frame_get_num_bodies(body_frame);
			cout << "# of bodies detected: " << num_bodies << "\n"; 
			if (num_bodies == 0) counter_no_body++;
			cout << counter_no_body << endl;

			for (size_t index = 0; index < num_bodies; index++)
			{
				uint32_t body_id = k4abt_frame_get_body_id(body_frame, index);
				//get joint information for each body
				k4abt_skeleton_t skeleton_info;
				k4abt_frame_get_body_skeleton(body_frame, index, &skeleton_info);
				cout << "w orientation: " << skeleton_info.joints->orientation.wxyz.w <<"\n";   //measured in normalized quaternion
				cout << "x orientation: " << skeleton_info.joints->orientation.wxyz.x << "\n";
				cout << "y orientation: " << skeleton_info.joints->orientation.wxyz.y << "\n";
				cout << "z orientation: " << skeleton_info.joints->orientation.wxyz.z << "\n";
				cout << "x position: " << skeleton_info.joints->position.xyz.x << "\n";   //measured in millimeters
				cout << "y position: " << skeleton_info.joints->position.xyz.y << "\n";
				cout << "z position: " << skeleton_info.joints->position.xyz.z << "\n";
			}

			k4abt_frame_release(body_frame);
		}

	}

	//close body tracker. stop & close camera
	k4abt_tracker_destroy(body_tracker);
	k4a_device_stop_cameras(kinect);
	k4a_device_close(kinect);
}
