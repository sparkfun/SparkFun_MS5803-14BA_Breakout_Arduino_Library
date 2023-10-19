/******************************************************************************
MS5803_I2C.cpp
Library for MS5803 pressure sensor.
Casey Kuhns @ SparkFun Electronics
6/26/2014
https://github.com/sparkfun/MS5803-14BA_Breakout

The MS58XX MS57XX and MS56XX by Measurement Specialties is a low cost I2C pressure
sensor.  This sensor can be used in weather stations and for altitude
estimations. It can also be used underwater for water depth measurements.

In this file are the functions in the MS5803 class

Resources:
This library uses the Arduino Wire.h to complete I2C transactions.

Development environment specifics:
	IDE: Arduino 1.0.5
	Hardware Platform: Arduino Pro 3.3V/8MHz
	MS5803 Breakout Version: 1.0

**Updated for Arduino 1.8.8 5/2019**

License: Please see LICENSE.md for more details.

Distributed as-is; no warranty is given.
******************************************************************************/

#include "SparkFun_MS5803_I2C.h"

MS5803::MS5803(ms5803_addr address)
// Base library type I2C
{
	_address = (uint8_t)address; // set interface used for communication
}

void MS5803::reset(void)
// Reset device I2C
{
	if (_i2cPort == nullptr)
		return;

	sendCommand(CMD_RESET);
	sensorWait(3);
}

uint8_t MS5803::begin(TwoWire &wirePort, uint8_t address)
{
	_address = address; // set interface used for communication
	return begin(wirePort);
}

uint8_t MS5803::begin(TwoWire &wirePort)
// Initialize library for subsequent pressure measurements
{
	_i2cPort = &wirePort; // Grab which port the user wants us to use

	reset(); // Reset the sensor to ensure the coefficients are loaded correctly

	uint8_t i;
	for (i = 0; i <= 7; i++)
	{
		sendCommand(CMD_PROM + (i * 2));
		_i2cPort->requestFrom(_address, (uint8_t)2);
		uint8_t highByte = _i2cPort->read();
		uint8_t lowByte = _i2cPort->read();
		coefficient[i] = (highByte << 8) | lowByte;
		// Uncomment below for debugging output.
		//	Serial.print("C");
		//	Serial.print(i);
		//	Serial.print("= ");
		//	Serial.println(coefficient[i]);
	}

	return 0;
}

float MS5803::getTemperature(temperature_units units, precision _precision)
// Return a temperature reading in either F or C.
{
	getMeasurements(_precision);
	float temperature_reported;
	// If Fahrenheit is selected return the temperature converted to F
	if (units == FAHRENHEIT)
	{
		temperature_reported = _temperature_actual / 100.0f;
		temperature_reported = (((temperature_reported)*9) / 5) + 32;
		return temperature_reported;
	}

	// If Celsius is selected return the temperature converted to C
	else
	{
		temperature_reported = _temperature_actual / 100.0f;
		return temperature_reported;
	}
}

float MS5803::getPressure(precision _precision)
// Return a pressure reading
{
	getMeasurements(_precision);
	float pressure_reported;
	pressure_reported = _pressure_actual;		   // Units: 0.1mbar
	pressure_reported = pressure_reported / 10.0f; // Convert to mbar (float)
	return pressure_reported;
}

void MS5803::getMeasurements(precision _precision)

{
	// Retrieve ADC result
	int32_t temperature_raw = getADCconversion(TEMPERATURE, _precision);
	int32_t pressure_raw = getADCconversion(PRESSURE, _precision);

	// Create Variables for calculations
	int32_t temp_calc;
	int32_t pressure_calc;

	int32_t dT;

	// Now that we have a raw temperature, let's compute our actual.
	dT = temperature_raw - ((int32_t)coefficient[5] << 8);
	temp_calc = (((int64_t)dT * coefficient[6]) >> 23) + 2000;

	// TODO TESTING  _temperature_actual = temp_calc;

	// Now we have our first order Temperature, let's calculate the second order.
	int64_t T2, OFF2, SENS2, OFF, SENS; // working variables

	if (temp_calc < 2000)
	// If temp_calc is below 20.0C
	{
		T2 = 3 * (((int64_t)dT * dT) >> 33);
		OFF2 = 3 * ((temp_calc - 2000) * (temp_calc - 2000)) / 2;
		SENS2 = 5 * ((temp_calc - 2000) * (temp_calc - 2000)) / 8;

		if (temp_calc < -1500)
		// If temp_calc is below -15.0C
		{
			OFF2 = OFF2 + 7 * ((temp_calc + 1500) * (temp_calc + 1500));
			SENS2 = SENS2 + 4 * ((temp_calc + 1500) * (temp_calc + 1500));
		}
	}
	else
	// If temp_calc is above 20.0C
	{
		T2 = 7 * ((uint64_t)dT * dT) / pow(2, 37);
		OFF2 = ((temp_calc - 2000) * (temp_calc - 2000)) / 16;
		SENS2 = 0;
	}

	// Now bring it all together to apply offsets

	OFF = ((int64_t)coefficient[2] << 16) + (((coefficient[4] * (int64_t)dT)) >> 7);
	SENS = ((int64_t)coefficient[1] << 15) + (((coefficient[3] * (int64_t)dT)) >> 8);

	temp_calc = temp_calc - T2;
	OFF = OFF - OFF2;
	SENS = SENS - SENS2;

	// Now lets calculate the pressure

	pressure_calc = (((SENS * pressure_raw) / 2097152) - OFF) / 32768;

	_temperature_actual = temp_calc;
	_pressure_actual = pressure_calc; // 10;// pressure_calc;
}

uint32_t MS5803::getADCconversion(measurement _measurement, precision _precision)
// Retrieve ADC measurement from the device.
// Select measurement type and precision
{
	if (_i2cPort == nullptr)
		return 0;
		
	uint32_t result;
	uint8_t highByte = 0, midByte = 0, lowByte = 0;

	sendCommand(CMD_ADC_CONV + _measurement + _precision);
	// Wait for conversion to complete
	sensorWait(1); // general delay
	switch (_precision)
	{
	case ADC_256:
		sensorWait(1);
		break;
	case ADC_512:
		sensorWait(3);
		break;
	case ADC_1024:
		sensorWait(4);
		break;
	case ADC_2048:
		sensorWait(6);
		break;
	case ADC_4096:
		sensorWait(10);
		break;
	}

	sendCommand(CMD_ADC_READ);
	_i2cPort->requestFrom(_address, (uint8_t)3);

	while (_i2cPort->available())
	{
		highByte = _i2cPort->read();
		midByte = _i2cPort->read();
		lowByte = _i2cPort->read();
	}

	result = ((uint32_t)highByte << 16) | ((uint32_t)midByte << 8) | lowByte;

	return result;
}

void MS5803::sendCommand(uint8_t command)
{
	if (_i2cPort == nullptr)
		return;
		
	_i2cPort->beginTransmission(_address);
	_i2cPort->write(command);
	_i2cPort->endTransmission();
}

void MS5803::sensorWait(uint8_t time)
// Delay function.  This can be modified to work outside of Arduino based MCU's
{
	delay(time);
}
