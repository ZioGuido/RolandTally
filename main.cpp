////////////////////////////////////////////////////////////////////////
// Roland V-8HD Tally Light Monitor / Decoder
// Code by Guido Scognamiglio - www.GenuineSoundware.com
// Last update: Mar 2022
//
// Needs RtMidi and wiringPi (RPi only)
//

/*
RPi GPIO MAP
 +-----+-----+---------+------+---+---Pi 2---+---+------+---------+-----+-----+
 | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 |     |     |    3.3v |      |   |  1 || 2  |   |      | 5v      |     |     |
 |   2 |   8 |   SDA.1 | ALT0 | 1 |  3 || 4  |   |      | 5V      |     |     |
 |   3 |   9 |   SCL.1 | ALT0 | 1 |  5 || 6  |   |      | 0v      |     |     |
 |   4 |   7 | GPIO. 7 |  OUT | 0 |  7 || 8  | 1 | ALT0 | TxD     | 15  | 14  |
 |     |     |      0v |      |   |  9 || 10 | 0 | OUT  | RxD     | 16  | 15  |
 |  17 |   0 | GPIO. 0 |  OUT | 0 | 11 || 12 | 1 | OUT  | GPIO. 1 | 1   | 18  |
 |  27 |   2 | GPIO. 2 |  OUT | 0 | 13 || 14 |   |      | 0v      |     |     |
 |  22 |   3 | GPIO. 3 |  OUT | 0 | 15 || 16 | 0 | OUT  | GPIO. 4 | 4   | 23  |
 |     |     |    3.3v |      |   | 17 || 18 | 0 | OUT  | GPIO. 5 | 5   | 24  |
 |  10 |  12 |    MOSI | ALT0 | 0 | 19 || 20 |   |      | 0v      |     |     |
 |   9 |  13 |    MISO | ALT0 | 0 | 21 || 22 | 0 | OUT  | GPIO. 6 | 6   | 25  |
 |  11 |  14 |    SCLK | ALT0 | 0 | 23 || 24 | 1 | OUT  | CE0     | 10  | 8   |
 |     |     |      0v |      |   | 25 || 26 | 1 | OUT  | CE1     | 11  | 7   |
 |   0 |  30 |   SDA.0 |   IN | 1 | 27 || 28 | 1 | IN   | SCL.0   | 31  | 1   |
 |   5 |  21 | GPIO.21 |   IN | 1 | 29 || 30 |   |      | 0v      |     |     |
 |   6 |  22 | GPIO.22 |   IN | 1 | 31 || 32 | 0 | IN   | GPIO.26 | 26  | 12  |
 |  13 |  23 | GPIO.23 |   IN | 0 | 33 || 34 |   |      | 0v      |     |     |
 |  19 |  24 | GPIO.24 |   IN | 0 | 35 || 36 | 0 | IN   | GPIO.27 | 27  | 16  |
 |  26 |  25 | GPIO.25 |   IN | 0 | 37 || 38 | 0 | IN   | GPIO.28 | 28  | 20  |
 |     |     |      0v |      |   | 39 || 40 | 0 | IN   | GPIO.29 | 29  | 21  |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
 +-----+-----+---------+------+---+---Pi 2---+---+------+---------+-----+-----+
*/


#include <stdio.h>

#ifdef WIN32
#include "windows.h"
#define WIN32_LEAN_AND_MEAN
#include <tchar.h>

// Linux
#else
#include "stdlib.h"
#include "unistd.h"
#include <wiringPi.h>
#endif

#include "RtMidi/RtMidi.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <cstring>
#include <iterator>

// Pin numbers. We're using the Broadcom chip pin numbers.
#define RELAY1	0	//	17
#define RELAY2	1	//	18
#define RELAY3	2	//	27
#define RELAY4	3	//	22
#define RELAY5	4	//	23
#define RELAY6	5	//	24
#define RELAY7	6	//	25
#define RELAY8	7	//	4
static const int relays[] = { RELAY1, RELAY2, RELAY3, RELAY4, RELAY5, RELAY6, RELAY7, RELAY8 };


// Get needed objects
RtMidiIn* MidiIn = new RtMidiIn();
RtMidiOut* MidiOut = new RtMidiOut();
int MidiInPortNum = -1;
int MidiOutPortNum = -1;
void OpenMidiPorts();

std::thread RunningThread;
std::atomic<bool> Running;
int TallyNumber = 0;
int TallyStatus[8] = { 0 };
int Prev_TallyStatus[8] = { -1 };
int TALLY = -1;

void LogSysex(std::vector<unsigned char>& midiData)
{
	std::ostringstream oss;
	std::copy(midiData.begin(), midiData.end(), std::ostream_iterator<int>(oss, " "));
	printf("SYSTEM EXCLUSIVE: %s\n", oss.str().c_str());
}

unsigned char GetRolandChecksum(std::vector<unsigned char>& midiData)
{
	int sum = 0;
	for (int i = 8; i <= 13; ++i) sum += midiData[i];
	return 128 - (sum % 128);
}

void PollSwitcher()
{
	Running.store(true);

	while (Running.load())
	{
		// Make a pause
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		if (MidiInPortNum < 0 || MidiOutPortNum < 0)
			continue;

		// Send request for the currently selected Tally number
		// F0 41 10 00 00 00 68 11 0c 00 xx 00 00 08 ck F7

		std::vector<unsigned char> syx;
		syx.push_back(0xF0);
		syx.push_back(0x41);
		syx.push_back(0x10);
		syx.push_back(0x00);
		syx.push_back(0x00);
		syx.push_back(0x00);
		syx.push_back(0x68);
		syx.push_back(0x11);
		syx.push_back(0x0C);
		syx.push_back(0x00);
		syx.push_back(TallyNumber);
		syx.push_back(0x00);
		syx.push_back(0x00);
		syx.push_back(0x08);
		syx.push_back(GetRolandChecksum(syx));
		syx.push_back(0xF7);

		//LogSysex(syx);

		// Check if MIDI OUT port is still available
		bool MidiOutOK = false;
		for (int p = 0; p < MidiOut->getPortCount(); ++p)
			if (MidiOut->getPortName(p).find("V-8HD") != std::string::npos)
				MidiOutOK = true;

		if (!MidiOutOK)
		{
			OpenMidiPorts();
			continue;
		}

		// Send message
		MidiOut->sendMessage(&syx);

		// Shift to next light
		if (++TallyNumber > 7)
			TallyNumber = 0;
	}
}

void ProcessSysexChunk(std::vector<unsigned char>& midiData)
{
	//LogSysex(midiData);

	if (midiData[7] != 0x12) return;

	auto tn = midiData[10];
	TallyStatus[tn] = midiData[11];

	if (Prev_TallyStatus[tn] != TallyStatus[tn])
	{
		Prev_TallyStatus[tn] = TallyStatus[tn];

		for (int t = 0; t < 8; ++t)
			if (TallyStatus[t] == 1)
				TALLY = t;

#ifdef WIN32
		printf("Tally = %d\r", TALLY);
#else
		for (int t = 0; t < 8; ++t)
			digitalWrite(relays[t], TALLY == t ? 0 : 1);
#endif
	}
}

void ProcessMidiInputCallback(double /*deltatime*/, std::vector<unsigned char>* message, void* /*userData*/)
{
	std::vector<unsigned char>& midiData = *message;	// Deference

	if (midiData.size() > 1)
	{
		// Sysex String
		if (midiData.back() == 0xF7)
			ProcessSysexChunk(midiData);
		// Event
		//else
		//if (midiData.back() < 0xF0) // not a real time event
		//	ProcessMidiEvent(midiData.at(0), midiData.at(1) & 0x7f, midiData.size() == 3 ? midiData.at(2) & 0x7f : 0);
	}
}

void myRtMidiErrorCallback(RtMidiError::Type /*type*/, const std::string& errorText)//, void*)
{
	printf("RtMidi says: %s\n", errorText.c_str());
}

void OpenMidiPorts()
{
	// Reset ports
	if (MidiIn->isPortOpen()) { MidiIn->closePort(); }
	if (MidiOut->isPortOpen()) { MidiOut->closePort(); }
	MidiInPortNum = MidiOutPortNum = -1;

	for (int p = 0; p < MidiIn->getPortCount(); ++p)
		if (MidiIn->getPortName(p).find("V-8HD") != std::string::npos)
			MidiInPortNum = p;

	for (int p = 0; p < MidiOut->getPortCount(); ++p)
		if (MidiOut->getPortName(p).find("V-8HD") != std::string::npos)
			MidiOutPortNum = p;

	//printf("Midi In: %d - Midi Out: %d\n", MidiInPortNum, MidiOutPortNum);

	if (MidiInPortNum < 0 || MidiOutPortNum < 0)
	{
#ifdef WIN32
		printf("ERROR: Roland V-8HD not connected.\n");
		return;
#else
		//printf("No Ports. Polling...\n");
		std::this_thread::sleep_for(std::chrono::seconds(1));
		OpenMidiPorts();
#endif
	}


	// Setup MIDI IN 
	if (MidiInPortNum >= 0 && !MidiIn->isPortOpen())
	{
		delete MidiIn; 
		MidiIn = new RtMidiIn();
		MidiIn->openPort(MidiInPortNum);
		MidiIn->ignoreTypes(false, true, true);	// must be set for accepting SysEx
		MidiIn->setCallback(ProcessMidiInputCallback, nullptr);
		MidiIn->setErrorCallback(myRtMidiErrorCallback);
	}

	// Setup MIDI OUT 
	if (MidiOutPortNum >= 0 && !MidiOut->isPortOpen())
	{
		delete MidiOut; 
		MidiOut = new RtMidiOut();
		MidiOut->openPort(MidiOutPortNum);
		MidiOut->setErrorCallback(myRtMidiErrorCallback);
	}
}

int main(int argc, char** argv)
{
	// For compiling this as a daemon that spawns a child process and runs in the background
#ifndef WIN32
	if (argc > 1 && strcmp(argv[1], "-d") == 0)
	{
		pid_t pid;
		pid = fork(); if (pid < 0) exit(EXIT_FAILURE); if (pid > 0) exit(EXIT_SUCCESS);
	}

	// Initialize wiringPi using Broadcom pin numbers
	wiringPiSetup();

	// Set GPIO outputs and reset relays
	for (int r = 0; r < 8; ++r)
	{
		pinMode(relays[r], OUTPUT);
		digitalWrite(relays[r], 1);
	}
#endif

	// Attempt to open Midi ports
	OpenMidiPorts();

	// Display status
	//cout << "Running on MIDI ports:\n";
	//cout << "IN: " << MidiIn->getPortName(MidiInPortNum) << "\n";
	//cout << "OUT: " << MidiOut->getPortName(MidiOutPortNum) << "\n";

	// Run Midi polling thread
	RunningThread = std::thread(&PollSwitcher);

#ifdef WIN32
	std::cout << "Press <enter> to quit.\n";
	char input; std::cin.get(input);
#else
	if (argc > 1 && strcmp(argv[1], "-d") == 0)
	{
		while (1) { sleep(50); }
	} else {
		cout << "Press <enter> to quit.\n";
		char input; cin.get(input);
	}
#endif

	// Quit
	Running.store(false);
	if (RunningThread.joinable())
		RunningThread.join();

	// Close MIDI IN port
	MidiIn->closePort();
	delete MidiIn;

	MidiOut->closePort();
	delete MidiOut;

	return 0;
}
