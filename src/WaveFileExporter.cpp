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
        // Copy the SAP-R bytes to memory for this frame
        streambuffer = pokeyStream.GetStreamBuffer() + frames * frameSize;

        // A frame is AUDF1, AUDC1 ... AUDF4, AUDC4, AUDCTL of a POKEY; in stereo
        // the 2nd POKEY ($D210, tracks 5-8) comes first, then the 1st ($D200)
        const byte* pokey1 = streambuffer + (frameSize == 18 ? 9 : 0);
        for (int i = 0; i < 4; i++) {
            memory[RMTPLAYR_TRACKN_AUDF + i] = pokey1[i * 2];
            memory[RMTPLAYR_TRACKN_AUDC + i] = pokey1[i * 2 + 1];
        }
        memory[RMTPLAYR_V_AUDCTL] = pokey1[8];
        if (frameSize == 18) {
            const byte* pokey2 = streambuffer;
            for (int i = 0; i < 4; i++) {
                memory[RMTPLAYR_TRACKN_AUDF + 4 + i] = pokey2[i * 2];
                memory[RMTPLAYR_TRACKN_AUDC + 4 + i] = pokey2[i * 2 + 1];
            }
            memory[RMTPLAYR_V_AUDCTL2] = pokey2[8];
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