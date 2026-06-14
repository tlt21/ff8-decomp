/**
 * @file snd_init.h
 * @brief Public interface for the sound engine driver (src/snd_init.c).
 *
 * Declares every function defined in snd_init.c: engine init/shutdown,
 * the @c sndCmd* SPU command dispatchers, sequence and SFX playback,
 * reverb/volume control, and the sample-bank upload/streaming path.
 * Shared sound types (@c SndBankDesc, @c SoundSeqTrack, @c DmaState, ...)
 * live in @c sound.h, which this header pulls in. @c sound.h re-includes
 * this header so existing @c #include "sound.h" callers keep resolving
 * these prototypes.
 */
#ifndef SND_INIT_H
#define SND_INIT_H

#include "sound.h"

/* ---- Documented public entry points -------------------------------- */

/**
 * @brief Play a one-shot SFX.
 *
 * @param sfxId  Sound effect id (rank-up = 0x5B..0x5D, Angelo learn = 0x83, etc.).
 * @param a1     Channel / mode flag (typically 0).
 * @param a2     Volume (typically 0x80).
 * @param a3     Pan (typically 0x7F).
 */
extern void sndPlaySfx(s32 sfxId, s32 a1, s32 a2, s32 a3);

/** @brief Play one SFX from a specific bank with explicit volume and pan. */
extern s32 sndPlayBankSfx(s32 bank, s32 idx, s32 vol, s32 pan);

/** @brief Stop a sequence by track pair (bank, channel) packed pair. */
extern void sndCmd21(s32 a0, s32 a1);

/** @brief Query the current active-channel mask for SFX dispatch. */
extern s32 func_800131A8(void);

/**
 * @brief SPU command-bus wrappers. Each writes its args into the
 *        command buffer at @c D_80075058 and tail-calls
 *        @c func_8001A1E8 to dispatch the indicated opcode. The
 *        SPU-handle / dispatch result is left in @c $v0 by the inner
 *        @c jal, which is why callers can treat them as returning
 *        @c s32 even though the body has no explicit @c return.
 */
extern s32 sndCmd10(s32 a0);
extern s32 sndCmd11(s32 a0);
extern s32 sndCmd12(s32 a0, s32 a1);
extern s32 sndCmd14(s32 a0, s32 a1, s32 a2);
extern s32 sndCmd19(s32 a0, s32 a1);
extern s32 sndCmd1A(s32 a0, s32 a1, s32 a2);
extern s32 sndCmdC0(s32 a0, s32 a1);
extern s32 sndCmdC1(s32 a0, s32 a1, s32 a2);
extern s32 sndCmdC2(s32 a0, s32 a1, s32 a2, s32 a3);

/** @brief Send SPU command @c 0x45 (no args). */
extern void sndCmd45(void);

/** @brief Reset SPU command bus (issued by the music-state reset opcode). */
extern void sndCmdF1(void);

/** @brief Set the global SPU master volume (0..0x7F). */
extern void sndSetMasterVolume(s32 vol);

/** @brief Set the global SEQ tempo (0x80 = normal). */
extern void sndSeqSetTempo(s32 tempo);

/** @brief Upload a staged sample bank to the SPU; returns -1 while busy. */
extern s32 sndUploadSamples(s32 a0, s32 a1);

/** @brief Query the sound engine's current busy state (0 = idle). */
extern s32 sndGetEngineState(void);

/* ---- Remaining engine / command / sequence functions --------------- */

extern s32 sndInit(void);
extern s32 sndShutdown(void);
extern s32 sndLoadBank(u8 *a0);
extern s32 sndStopAll(void);
extern s32 sndGetStatus(void);
extern s32 sndGetMaxVolume(s32 a0);
extern void sndCmd40(void);
extern s32 func_80012FEC(void);
extern void sndKeyOn(s32 a0);
extern void sndStopPlayback(void);
extern s32 func_80013210(s32 a0);
extern void sndSelectMode(s32 a0);
extern void sndCmd90(s32 a0);
extern void sndCmd92(s32 a0);
extern void sndEnableReverb(u32 a0);
extern void sndDisableReverb(u32 a0);
extern s32 func_800133D8(s32 arg0);
extern s32 sndSetEngineFlag(s32 a0);
extern void sndSetChannelVolume(s32 a0, s32 a1);
extern void sndSeqPlay7bit(s32 a0, s32 a1, s32 a2);
extern void sndSeqPlayPan7bit(s32 a0, s32 a1, s32 a2, s32 a3);
extern void sndSeqSetChannelTempo(s32 a0, s32 a1);
extern void sndSeqPlay8bit(s32 a0, s32 a1, s32 a2);
extern void sndSeqPlayPan8bit(s32 a0, s32 a1, s32 a2, s32 a3);
extern void sndSeqSetTempoAlt(s32 a0);
extern void sndSeqSetChannelTempoAlt(s32 a0, s32 a1);
extern void sndSeqStart(s32 a0, s32 a1, s32 a2);
extern void sndSeqStartPan(s32 a0, s32 a1, s32 a2, s32 a3);
extern void sndCmdC8(s32 a0);
extern void sndCmdC9(s32 a0, s32 a1);
extern void sndCmdCA(s32 a0, s32 a1, s32 a2);
extern void sndCmdD0(s32 a0);
extern void sndCmdD1(s32 a0, s32 a1);
extern void sndCmdD2(s32 a0, s32 a1, s32 a2);
extern void sndCmdD4(s32 a0);
extern void sndCmdD5(s32 a0, s32 a1);
extern void sndCmdD6(s32 a0, s32 a1, s32 a2);
extern void sndCmdD8(s32 a0);
extern void sndCmdD9(s32 a0, s32 a1);
extern void sndCmdDA(s32 a0, s32 a1, s32 a2);
extern void sndCmdF0(void);
extern s32 func_80013AA8(SndBankDesc *a0, s32 *val1, s32 *val2);
extern s32 sndResetState(void);
extern s32 func_80013CD4(s32 a0, u32 a1, s32 a2);
extern s32 sndProcessAudio(s32 a0, s32 a1);
extern s32 func_80013F38(SndBankDesc *a0, s32 a1);
extern void sndSetPlaybackAddr(s32 a0, s32 a1);
extern void func_80014094(SndBankDesc *a0, s32 a1, s32 a2);
extern void func_80014190(SndBankDesc *a0, s32 a1, s32 a2);
extern s32 sndSetCdMixVolume(s32 a0);
extern void sndCmdE0(s32 a0, s32 a1, s32 a2);
extern void sndCmdE2(void);
extern void sndCmdE4(s32 a0);
extern void sndCmdE5(s32 a0, s32 a1);
extern void sndCmdE6(s32 a0);
extern s32 func_80014400(s32 a0, s32 a1);
extern void sndCmdED(s32 a0, s32 a1);
extern void func_8001451C(s32 a0, s32 a1, s32 a2, s32 a3);
extern s32 sndInitIrq(s32 a0, s32 a1);
extern s32 sndTickCounters(void);

#endif /* SND_INIT_H */
