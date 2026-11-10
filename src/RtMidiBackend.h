//
// RtMidiBackend.h - RtMidi cross-platform backend
//
// Implements IMidiBackend using RtMidi library
// Available on: Windows, Linux, macOS
//

#pragma once

#ifndef RTMIDIBACKEND_H
#define RTMIDIBACKEND_H

#include "MidiBackend.h"
#include <string>
#include <vector>

#ifdef RTMIDI_AVAILABLE
// The real types: RtMidi 6.1 declares RtMidiIn/RtMidiOut in the namespace rt::midi and brings them out with
// using-declarations, so a forward declaration here would be a second, different class (ambiguous with 6.1)
#include <RtMidi.h>
#else
// Forward declarations (the backend is not built without RtMidi)
class RtMidiOut;
class RtMidiIn;
#endif

/**
 * @brief RtMidi backend implementation
 * 
 * Provides cross-platform MIDI I/O via RtMidi.
 * Works on Windows, Linux, and macOS.
 * 
 * Benefits:
 * - Same API across all platforms
 * - ALSA on Linux
 * - WASAPI on Windows
 * - CoreMIDI on macOS
 */
class RtMidiBackend : public IMidiBackend {
public:
    RtMidiBackend();
    virtual ~RtMidiBackend();

    bool Init() override;
    void Deinit() override;
    bool Reinit() override;

    int GetPortCount() const override;
    std::string GetPortName(int portIndex) const override;
    bool OpenPort(int portIndex) override;
    void ClosePort() override;
    bool IsPortOpen() const override;

    bool SendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override;
    bool SendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override;
    bool SendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override;
    bool SendProgramChange(uint8_t channel, uint8_t program) override;
    size_t SendRawMessage(const uint8_t* data, size_t length) override;

    bool PollMidiInput(MidiEvent& event) override;
    const char* GetErrorMessage() const override;
    bool IsReady() const override;

private:
    // RtMidi objects (opaque pointers, real types only visible in .cpp)
    RtMidiOut* m_midiOut; ///< RtMidiOut instance
    RtMidiIn* m_midiIn;   ///< RtMidiIn instance

    int m_port_count;
    int m_current_port;
    bool m_is_initialized;
    std::string m_error_message;

    // Cached port names
    std::vector<std::string> m_port_names;

    // Helper methods
    bool InitRtMidi();
    void DeinitRtMidi();
    bool EnumeratePorts();
};

#endif // RTMIDIBACKEND_H
