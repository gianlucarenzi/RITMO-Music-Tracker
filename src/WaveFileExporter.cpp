#include "PlatformTypes.h"
#include "WaveFileExporter.h"
#include "WaveFile.h"
#include "GuiHelpers.h"
#include "LZSSFile.h"
#include "AtariTrackerDriver.h"
#include "ChannelControl.h"
#include "AtariTrackerDriver.h"

extern CAtariTrackerDriver* g_AtariTrackerDriver;

bool CWaveFileExporter::ExportWAV(CSongExport& songExport, std::ofstream& ou, CXPokey& pokey, byte* memory)
{
    CWaveFile wavefile{};

    BYTE* buffer = NULL;
    BYTE* streambuffer = NULL;
    const WAVEFORMATEX* wfm = NULL;
    int length = 0, frames = 0;
    const int frameSize = CLZSSFile::GetFrameSize(songExport.GetSong());

    ou.close(); // hack, just to be able to actually use the filename for now...

    if (!(wfm = pokey.GetSoundFormat()) || !wfm->nSamplesPerSec) {
        SendErrorMessage("Wave Export Failed", "Could not get sound format!");
        return false;
    }

    if (!wavefile.OpenFile(songExport.GetFilePath().GetBuffer(), wfm->nSamplesPerSec, wfm->wBitsPerSample, wfm->nChannels)) {
        SendErrorMessage("Wave Export Failed", "Could not create the WAV file!");
        return false;
    }

    // Dump the POKEY registers from full song playback
    CPokeyStream& pokeyStream = songExport.GetSongContainer().GetModifiablePokeyStream();

    // Busy writing: the timer routine must not play or render the POKEY
    // meanwhile (another thread, same Atari memory and POKEY emulation)
    pokeyStream.SetState(CPokeyStream::WRITE);
    songExport.GetSong().SetStreamRendering(&pokeyStream);

    g_AtariTrackerDriver->Init(); // Reset the Atari memory
    SetChannelOnOff(-1, 1);       // Unmute all channels

    // Create the sound buffer to copy from and to
    auto bufferSize = CXPokey::BUFFER_SIZE;
    buffer = new BYTE[CXPokey::BUFFER_SIZE];
    memset(buffer, 0x80, bufferSize);

    pokey.ResetRenderCalls();
    pokey.ResetPokeys(); // the timer may have run the emulation since the last export
    while (frames < pokeyStream.GetFirstCountPoint()) {
        // Copy the SAP-R bytes of this frame straight to the POKEY registers at $D200/$D210, where
        // CopyAtariMemoryToPokey() reads them. They went into the Patch 16 driver's variables before, and the loaded
        // driver's SetPokey carried them there: with a v1-family driver that overwrote the player (TRACKN_AUDC is
        // $3180-$3187, across its tabbeganddistor at $3182) and rendered the driver's own variables rather than the
        // stream (ported from RMT 1.36, plan 46, 5694184). A frame is AUDF1, AUDC1 ... AUDF4, AUDC4, AUDCTL of a
        // POKEY; in stereo the 2nd POKEY ($D210, tracks 5-8) comes first, then the 1st ($D200).
        streambuffer = pokeyStream.GetStreamBuffer() + frames * frameSize;
        if (frameSize == 18) {
            for (int i = 0; i < 9; i++) {
                memory[0xd200 + i] = streambuffer[0x09 + i];
                memory[0xd210 + i] = streambuffer[0x00 + i];
            }
        } else {
            for (int i = 0; i < 9; i++) memory[0xd200 + i] = streambuffer[i];
        }

        // Fill the POKEY buffer with the sound of this frame: one call of the driver, 1/instrument speed of a VBI
        // (rendering a whole VBI for every frame made the WAV of a song at instrument speed 4 four times as long)
        pokey.RenderSoundV2Call(songExport.GetSong().GetInstrumentSpeed(), buffer, length);

        // Write the buffer to WAV file
        wavefile.WriteWave(buffer, length);

        // Update the PokeyStream offset for the next frame
        frames++;
    }

    SetChannelOnOff(-1, 0); // Mute all channels

    // The timer routine may play again
    songExport.GetSong().SetStreamRendering(nullptr);
    pokeyStream.SetState(CPokeyStream::STOP);

    // Finished doing WAV things...
    wavefile.CloseFile();

    // Also make sure to delete the buffer once it's no longer needed
    delete[] buffer;

    return true;
}