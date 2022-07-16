/* -*- Mode: ObjC; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; fill-column: 150 -*- */

// compile with clang virtualmidi.mm  -framework CoreMIDI -framework Cocoa -lstdc++ -o vm 


//#import <array>
//#import <cassert>
//#import <cstdio>

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMidi/CoreMidi.h>

#import "midi.h"

static CGEventSourceRef eventSource;
MIDIClientRef midi_client;
MIDIPortRef out_port;


static void NotifyProc(const MIDINotification *message, void *refCon)
{
}


void midi_out(uint8_t * bytes, uint16_t len) {

}

void* run_midi(void*argp){
    @autoreleasepool {
        py_midi_callback = 0;
        last_midi_len = 0;

        OSStatus status = MIDIClientCreate((__bridge CFStringRef)@"Tulip", NotifyProc, NULL, &midi_client);
        if (status != noErr) {
            fprintf(stderr, "Error %d while setting up handlers\n", status);
        }


        status = MIDIOutputPortCreate(midi_client, (__bridge CFStringRef)[NSString stringWithFormat:@"Tulip Output"], &out_port );
        if(status != noErr) {
            fprintf(stderr, "Error %d while setting up MIDI output port\n", status);
        }

        eventSource = CGEventSourceCreate(kCGEventSourceStatePrivate);
        ItemCount number_sources = MIDIGetNumberOfSources();
        for (int i = 0; i < number_sources; i++) {
            MIDIEndpointRef source = MIDIGetSource(i);
            MIDIPortRef in_port;
            status = MIDIInputPortCreateWithProtocol(midi_client, (__bridge CFStringRef)[NSString stringWithFormat:@"Tulip Input %d", i], kMIDIProtocol_1_0, &in_port, ^(const MIDIEventList *evtlist, void *srcConnRefCon) {
                const MIDIEventPacket* packet = &evtlist->packet[0];
                for (int i = 0; i < evtlist->numPackets; i++) {
                    if(packet->wordCount == 1){
                        const unsigned char *bytes = (unsigned char*)(&packet->words[0]);
                        if(bytes[3] == 0x20) { //  TODO, non-3 packets, then what? 
                            last_midi[0] = bytes[2];
                            last_midi[1] = bytes[1];
                            last_midi[2] = bytes[0];
                            last_midi_len = 3;
                            if(py_midi_callback) tulip_midi_isr();
                            packet = MIDIEventPacketNext(packet);
                        }
                    }
                }
            });
            if (status != noErr) {
                fprintf(stderr, "Error %d while setting up MIDI input port\n", status);
            }
            status = MIDIPortConnectSource(in_port, source, NULL);
            if (status != noErr) {
                fprintf(stderr, "Error %d while connecting MIDI input port to source\n", status);
            }
        }
        CFRunLoopRun();
    }
    return NULL;
}