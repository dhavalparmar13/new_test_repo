#ifndef __MOD_GOOGLE_ASR_H__
#define __MOD_GOOGLE_ASR_H__

#include <switch.h>
#include <speex/speex_resampler.h>

#include <unistd.h>

#define MAX_SESSION_ID (256)

#define MY_BUG_NAME "google_asr"
#define TRANSCRIBE_EVENT_RESULTS "google_asr::transcription"
#define TRANSCRIBE_EVENT_END_OF_UTTERANCE "google_asr::end_of_utterance"
#define TRANSCRIBE_EVENT_START_OF_TRANSCRIPT "google_asr::start_of_transcript"
#define TRANSCRIBE_EVENT_END_OF_TRANSCRIPT "google_asr::end_of_transcript"
#define TRANSCRIBE_EVENT_NO_AUDIO_DETECTED "google_asr::no_audio_detected"
#define TRANSCRIBE_EVENT_MAX_DURATION_EXCEEDED "google_asr::max_duration_exceeded"
#define TRANSCRIBE_EVENT_PLAY_INTERRUPT "google_asr::play_interrupt"


// simply write a wave file
//#define DEBUG_TRANSCRIBE 0


#ifdef DEBUG_TRANSCRIBE

/* per-channel data */
struct cap_cb {
	switch_buffer_t *buffer;
	switch_mutex_t *mutex;
	char *base;
    SpeexResamplerState *resampler;
    FILE* fp;
};
#else
/* per-channel data */
typedef void (*responseHandler_t)(switch_core_session_t* session, const char* json);

struct cap_cb {
	switch_mutex_t *mutex;
	char sessionId[MAX_SESSION_ID];
	char *base;
  SpeexResamplerState *resampler;
	void* streamer;
	responseHandler_t responseHandler;
	switch_thread_t* thread;
	int end_of_utterance;
	int play_file;
};
#endif

#endif
