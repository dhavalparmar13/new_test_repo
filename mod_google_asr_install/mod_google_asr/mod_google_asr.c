#include <switch.h>
#include <string.h>
#include <stdlib.h>
#include <switch_cJSON.h>
#include <curl/curl.h>
#include <time.h>
#include "cJSON/cJSON.h"
#include "mod_google_asr.h"
#include "google_asr.h"
#define WAVE_FILE_LENGTH 126
// 126 - 16(/time.pcm)
#define WAVE_DIR_MAX_LENGTH 110

//struture for the google configuration file
struct {
    char *google_application_crdentials;
    char *google_default_lang_parameter;
    int google_default_interim_parameter;
    int   log_level;
}globals;

switch_channel_t *user_channel;
static char * uuid;
char *google_lang = NULL;
unsigned int sampling_rate;
int max_alternatives_parameter = 0;
char *language_parameter = NULL;
clock_t start, end;

SWITCH_MODULE_LOAD_FUNCTION(mod_google_asr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_google_asr_shutdown);
SWITCH_MODULE_DEFINITION(mod_google_asr, mod_google_asr_load, mod_google_asr_shutdown, NULL);

static switch_status_t do_stop(switch_core_session_t *session);

typedef enum {
	ASRFLAG_READY = (1 << 0),
	ASRFLAG_INPUT_TIMERS = (1 << 1),
	ASRFLAG_START_OF_SPEECH = (1 << 2),
	ASRFLAG_RETURNED_START_OF_SPEECH = (1 << 3),
	ASRFLAG_NOINPUT_TIMEOUT = (1 << 4),
	ASRFLAG_RESULT = (1 << 5),
	ASRFLAG_RETURNED_RESULT = (1 << 6),
	ASRFLAG_RECOGNITION = (1 << 7)
} google_asr_flag_t;

typedef struct {
	uint32_t flags;
	const char *result_text;
	char *file;
	double result_confidence;
	uint32_t thresh;
	uint32_t silence_ms;
	uint32_t voice_ms;
	int no_input_timeout;
	int speech_timeout;
        int dtmf_digits;
        char final_dtmf_digits[100];
	switch_bool_t start_input_timers;
	switch_time_t no_input_time;
	switch_time_t speech_time;
	char *grammar;
	char *channel_uuid;
	switch_vad_t *vad;
} google_asr_t;

static void google_asr_reset(google_asr_t *context)
{
        if(globals.log_level == 7 || globals.log_level == 5)
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Restting google_asr .\n");
	if (context->vad) {
		switch_vad_reset(context->vad);
	}
	context->flags = 0;
	context->result_text = "--";
	context->result_confidence = 0.0;
        
	switch_set_flag(context, ASRFLAG_READY);
	context->no_input_time = switch_micro_time_now();
	if (context->start_input_timers) {
		switch_set_flag(context, ASRFLAG_INPUT_TIMERS);
	}
}


static switch_status_t google_load_config(void)
{
          char * google_application_crdential;
          int log_level_parameter;
          char * default_value = "";
	  char * uuid_Pass;
          static const char *global_cf = "google.conf"; /*! Config File Name */

          switch_status_t status = SWITCH_STATUS_SUCCESS;     /*! Loading Config Status */
          switch_xml_t cfg, xml, settings, param;             /*! XML Config Params */


        if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL)))
        {
          switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ ROUTING ] : Open of %s failed\n", global_cf);
          status = SWITCH_STATUS_TERM;
        }

        if ((settings = switch_xml_child(cfg, "settings")))
        {

                /*** @Section Load config param into variable */

                for (param = switch_xml_child(settings, "param"); param; param = param->next){
                        char *var = (char *) switch_xml_attr_soft(param, "name");
                        char *val = (char *) switch_xml_attr_soft(param, "value");
                        if (!strcasecmp(var, "google_application_crdentials")){
                             globals.google_application_crdentials = strdup(val);
                             switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[ GOOGLE_TRANSCRIBE ] : google_credentials [ %s ]\n", globals.google_application_crdentials);
                        }                        
		        if (!strcasecmp(var, "google_default_language")){
                             globals.google_default_lang_parameter = strdup(val);
                             switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[ GOOGLE_TRANSCRIBE ] : google_default_language_parameter [ %s ]\n", globals.google_default_lang_parameter);
                        }
                        if (!strcasecmp(var, "google_default_interim_parameter")){
                             globals.google_default_interim_parameter = atoi(val);
                             switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[ GOOGLE_TRANSCRIBE ] : google_default_interim_parameter [ %d ]\n", globals.google_default_interim_parameter);
                        }
                        else if (!strcasecmp(var, "log-level")){
                                if(!strcasecmp(val,"CONSOLE")){globals.log_level = 0;}
                                else if(!strcasecmp(val,"ALERT")){globals.log_level = 1;}
                                else if(!strcasecmp(val,"CRIT")){globals.log_level = 2;}
                                else if(!strcasecmp(val,"ERR")){globals.log_level = 3;}
                                else if(!strcasecmp(val,"WARNING")){globals.log_level = 4;}
                                else if(!strcasecmp(val,"NOTICE")){globals.log_level = 5;}
                                else if(!strcasecmp(val,"INFO")){globals.log_level = 6;}
                                else{globals.log_level = 7;
                                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[ GOOGLE_TRANSCRIBE ] : No Log-level configured setting Default log-level for the module DEBUG \n");
                                }
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[ GOOGLE_TRANSCRIBE ] : log-level [ %d ]\n", globals.log_level);
                        }
                }
        }
        if(globals.google_application_crdentials == NULL){
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[ GOOGLE_TRANSCRIBE ] : Configuration Loading Unsuccessfull(mandatory parameters(globals.google_application_crdentials) are missing\n");
                        return SWITCH_STATUS_FALSE;
        }
        if((strcasecmp(globals.google_application_crdentials, default_value ) == 0)){
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ GOOGLE_TRANSCRIBE ] : Configuration Loading Unsuccessfull(mandatory parameters(globals.google_application_crdentials) are missing\n");
                        return SWITCH_STATUS_FALSE;
        }
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[ GOOGLE_TRANSCRIBE ] : Loaded Configuration successfully\n");
        google_application_crdential   = globals.google_application_crdentials;
        log_level_parameter = globals.log_level;
	uuid_Pass = uuid;
	//google_speech_load(google_application_crdential,log_level_parameter,uuid_Pass);
        google_speech_init(google_application_crdential,log_level_parameter,uuid_Pass);
        
        if (xml){
          switch_xml_free(xml);
        }
        return status;
}

static void responseHandler(switch_core_session_t* session, const char * json) {
        switch_event_t *event;
        switch_channel_t *channel = switch_core_session_get_channel(session);       
        switch_channel_set_variable(channel, "transcription_data", json);
       
        //if(globals.log_level == 7 || globals.log_level == 5)
               // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Received Json payload in Response handler:[%s] \n", json);
        if (0 == strcmp("end_of_utterance", json)) {
                switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_END_OF_UTTERANCE);
                switch_channel_event_set_data(channel, event);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "google");
        }
        else if (0 == strcmp("end_of_transcript", json)) {
                switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_END_OF_TRANSCRIPT);
                switch_channel_event_set_data(channel, event);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "google");
        }
        else if (0 == strcmp("start_of_transcript", json)) {
                switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_START_OF_TRANSCRIPT);
                switch_channel_event_set_data(channel, event);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "google");
        }
        else if (0 == strcmp("max_duration_exceeded", json)) {
                switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_MAX_DURATION_EXCEEDED);
                switch_channel_event_set_data(channel, event);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "google");
        }
        else if (0 == strcmp("no_audio", json)) {
                switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_NO_AUDIO_DETECTED);
                switch_channel_event_set_data(channel, event);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "google");
        }
        else if (0 == strcmp("play_interrupt", json)){
                switch_event_t *qevent;
                switch_status_t status;
                if (switch_event_create(&qevent, SWITCH_EVENT_DETECTED_SPEECH) == SWITCH_STATUS_SUCCESS) {
                        if ((status = switch_core_session_queue_event(session, &qevent)) != SWITCH_STATUS_SUCCESS){
                          if(globals.log_level == 7 || globals.log_level == 5)
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unable to queue play inturrupt event  %d \n", status);
                        }
                }else{
                      if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unable to create play inturrupt event \n");
                }
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_PLAY_INTERRUPT);
                switch_channel_event_set_data(channel, event);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "google");
        }
        else {
                if(globals.log_level == 7)
                 // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "json payload: %s.\n", json);
                switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_RESULTS);
                switch_channel_event_set_data(channel, event);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "google");
                switch_event_add_body(event, "%s", json);
        }
        switch_event_fire(&event);
}

static switch_bool_t capture_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
        switch_core_session_t *session = switch_core_media_bug_get_session(bug);
        switch (type) {
        case SWITCH_ABC_TYPE_INIT:
                        if(globals.log_level == 7 || globals.log_level == 6)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got SWITCH_ABC_TYPE_INIT.\n");
                        responseHandler(session, "start_of_transcript");
                break;
        case SWITCH_ABC_TYPE_CLOSE:
                {
                        if(globals.log_level == 7 || globals.log_level == 6)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got SWITCH_ABC_TYPE_CLOSE.\n");
                        responseHandler(session, "end_of_transcript");
                        google_speech_session_cleanup(session, 1);
                        if(globals.log_level == 7 || globals.log_level == 6)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Finished SWITCH_ABC_TYPE_CLOSE.\n");
                }
                break;

        case SWITCH_ABC_TYPE_READ:
                return google_speech_frame(bug, user_data);
                break;

        case SWITCH_ABC_TYPE_WRITE:
        default:
                if(globals.log_level == 7 || globals.log_level == 5)
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "No type received..\n");
                break;
        }
        return SWITCH_TRUE;
}

static switch_status_t do_stop(switch_core_session_t *session)
{
        switch_status_t status = SWITCH_STATUS_SUCCESS;
        switch_channel_t *channel = switch_core_session_get_channel(session);
        switch_media_bug_t *bug = switch_channel_get_private(channel, MY_BUG_NAME);
        if (bug) {
                if(globals.log_level == 7 || globals.log_level == 6)
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Received user command to stop transcription.\n");
                status = google_speech_session_cleanup(session, 0);
                if(globals.log_level == 7 || globals.log_level == 6)
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "stopped transcription.\n");
        }

        return status;
}

static switch_status_t start_capture(switch_core_session_t *session, switch_media_bug_flag_t flags,char* lang, int interim)
{
        switch_media_bug_t *bug;
        switch_status_t status;
        switch_codec_implementation_t read_impl = { 0 };
        void *pUserData;
        uint32_t samples_per_second;
        int single_utterance = 0, separate_recognition = 0, max_alternatives = 0, profanity_filter = 0, word_time_offset = 0, punctuation = 0, enhanced = 0;
        char* hints = NULL;
        char* model = NULL;
        switch_channel_t *channel = switch_core_session_get_channel(session);
        
        if(globals.log_level == 7 || globals.log_level == 6)
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,"start_capture started for transcription");

        if (switch_channel_get_private(channel, MY_BUG_NAME)) {
                if(globals.log_level == 7)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing bug from previous transcribe\n");
                
                do_stop(session);
        }
        
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_SINGLE_UTTERANCE"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                 switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling single_utterance[GOOGLE_SPEECH_SINGLE_UTTERANCE]");
                
                single_utterance = 1;
        }

        // transcribe each separately?
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_SEPARATE_RECOGNITION_PER_CHANNEL"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling separate_recognition[GOOGLE_SPEECH_SEPARATE_RECOGNITION_PER_CHANNEL]");
               
                separate_recognition = 1;
        }

        // max alternatives
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_MAX_ALTERNATIVES"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling max_alternatives[GOOGLE_SPEECH_MAX_ALTERNATIVES]");
               
                max_alternatives = atoi(switch_channel_get_variable(channel, "GOOGLE_SPEECH_MAX_ALTERNATIVES"));
        }

        // profanity filter
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_PROFANITY_FILTER"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                 switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling profanity_filter[GOOGLE_SPEECH_PROFANITY_FILTER]");
                
                 profanity_filter = 1;
         }

        // enable word offsets
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_ENABLE_WORD_TIME_OFFSETS"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling word_time_offset[GOOGLE_SPEECH_ENABLE_WORD_TIME_OFFSETS]");
                
                word_time_offset = 1;
    }

        // enable automatic punctuation
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_ENABLE_AUTOMATIC_PUNCTUATION"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                 switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling punctuation[GOOGLE_SPEECH_ENABLE_AUTOMATIC_PUNCTUATION]");
                punctuation = 1;
        }

    // speech model
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_MODEL"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling model[GOOGLE_SPEECH_MODEL]");
               
                model = (char *)switch_channel_get_variable(channel, "GOOGLE_SPEECH_MODEL");
        }

        // use enhanced model
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_USE_ENHANCED"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                  switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling enhanced[GOOGLE_SPEECH_USE_ENHANCED]");
                
                enhanced = 1;
        }

        // hints
        if (switch_true(switch_channel_get_variable(channel, "GOOGLE_SPEECH_HINTS"))) {
                if(globals.log_level == 7 || globals.log_level == 5)
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,"Enabling hints[GOOGLE_SPEECH_HINTS]");
                
                hints = (char *)switch_channel_get_variable_dup(channel, "GOOGLE_SPEECH_HINTS", SWITCH_TRUE, -1);
        }
        switch_core_session_get_read_impl(session, &read_impl);
        if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
                return SWITCH_STATUS_FALSE;
        }

        samples_per_second = !strcasecmp(read_impl.iananame, "g722") ? read_impl.actual_samples_per_second : read_impl.samples_per_second;
        
        if(globals.log_level == 7 )
	        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,"in start_capture before calling speech_init function .......");

        if (SWITCH_STATUS_FALSE == google_speech_session_init(session, responseHandler, samples_per_second, flags & SMBF_STEREO ? 2 : 1, lang, interim,single_utterance,separate_recognition, max_alternatives, profanity_filter, word_time_offset, punctuation, model, enhanced, hints, NULL, &pUserData)){
                if(globals.log_level == 7 || globals.log_level == 3)
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error initializing google_speech_session_init. \n");
                return SWITCH_STATUS_FALSE;
        }
        
        if ((status = switch_core_media_bug_add(session, "google_asr", NULL, capture_callback, pUserData, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS) {
                return status;
        }
        
        switch_channel_set_private(channel, MY_BUG_NAME, bug);
        
        return SWITCH_STATUS_SUCCESS;
}


/*
 * function to open the asr interface 
 * invoke once when start the asr engine
*/
static switch_status_t google_asr_open(switch_asr_handle_t *ah, const char *codec, int rate, const char *dest, switch_asr_flag_t *flags)
{
	google_asr_t *context;
	char *default_value = "";
	char *lang = NULL;
        int interim;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_media_bug_flag_t flag = SMBF_READ_STREAM /* | SMBF_WRITE_STREAM | SMBF_READ_PING */;
	switch_core_session_t *lsession = NULL;
        lsession = switch_core_session_locate(uuid);
	interim = globals.google_default_interim_parameter;
	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
                if(globals.log_level == 7 || globals.log_level == 3)
		        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[SWITCH_ASR_FLAG_CLOSED] asr_open attempt on CLOSED asr handle\n");
		
                return SWITCH_STATUS_FALSE;
	}

	if (!(context = (google_asr_t *) switch_core_alloc(ah->memory_pool, sizeof(*context)))) {
		return SWITCH_STATUS_MEMERR;
	}

	if(globals.log_level == 7 || globals.log_level == 6){        
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[google_asr_open] codec = %s, rate = %d, dest = %s\n", codec, rate, dest);
        }

	ah->private_info = context;
	ah->codec = switch_core_strdup(ah->memory_pool, codec);
	
        if (rate > 16000) {
		ah->native_rate = 16000;
	}

	context->thresh = 400;
	context->silence_ms = 700;
	context->voice_ms = 60;
	context->start_input_timers = 1;
	context->no_input_timeout = 5000;
	context->speech_timeout = 10000;

	context->vad = switch_vad_init(ah->native_rate, 1);
	switch_vad_set_mode(context->vad, -1);
	switch_vad_set_param(context->vad, "thresh", context->thresh);
	switch_vad_set_param(context->vad, "silence_ms", context->silence_ms);
	switch_vad_set_param(context->vad, "voice_ms", context->voice_ms);
	switch_vad_set_param(context->vad, "debug", 0);

	google_asr_reset(context);
        if(google_lang != NULL){
                lang = google_lang;
                if(globals.log_level == 7 || globals.log_level == 6)          
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[ GOOGLE_ASR ] :setting language from dialplan  %s\n",lang);
        }

        if(google_lang == NULL || google_lang == default_value){
	        lang = globals.google_default_lang_parameter;        
                if(globals.log_level == 7 || globals.log_level == 6)
	                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[google_asr_open] setting lang from configuration :%s\n",lang);
        }

       language_parameter = lang;
       sampling_rate = rate;

       // Starting Timer for Speech Feed.       
       start = clock();
       if(lsession==NULL){
               if(globals.log_level == 7 || globals.log_level == 3)
                      switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_ERROR, "No-Session found, Unable to start capture audio to transcribe \n");
        }else{
               if(globals.log_level == 7 || globals.log_level == 6)
                        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "Session found, starting capture to transcribe \n");               
	       start_capture(lsession, flag, lang, interim);
	       
               if(status == SWITCH_STATUS_SUCCESS);
                 if(globals.log_level == 7 || globals.log_level == 6)
                        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "starting capture to transcribe done \n");

        }
       
        switch_core_session_rwunlock(lsession);
    	return SWITCH_STATUS_SUCCESS;
}

/*! function to close the asr interface */
static switch_status_t google_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
        google_asr_t *context = (google_asr_t *)ah->private_info;
        switch_status_t status = SWITCH_STATUS_SUCCESS;	
	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
                if(globals.log_level == 7 || globals.log_level == 3)
		        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Double ASR close!\n");
	        
                return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_NOTICE, "ASR closing ...\n");
	if (context->vad) {
	        switch_vad_destroy(&context->vad);
	}
	switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);
	//switch_core_session_rwunlock(lsession);
	return status;
}

/*! function to feed audio to the ASR */
static switch_status_t google_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags){
        google_asr_t *context = (google_asr_t *) ah->private_info;      
        switch_status_t status = SWITCH_STATUS_SUCCESS;
        switch_vad_state_t vad_state;
	if (switch_test_flag(ah, ASRFLAG_RECOGNITION)) {
                if(globals.log_level == 7 || globals.log_level == 5)
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, " Recieved ASRFLAG_RECOGNITION.\n");
                return SWITCH_STATUS_BREAK;
        }

        if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
                if(globals.log_level == 7 || globals.log_level == 5)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Recieved SWITCH_ASR_FLAG_CLOSED.\n");
		return SWITCH_STATUS_BREAK;
	}

	if (switch_test_flag(context, ASRFLAG_RETURNED_RESULT) && switch_test_flag(ah, SWITCH_ASR_FLAG_AUTO_RESUME)) {
                if(globals.log_level == 7)
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Auto Resuming\n");
		google_asr_reset(context);
	}

	if (switch_test_flag(context, ASRFLAG_READY)){
                vad_state = switch_vad_process(context->vad, (int16_t *)data, len / sizeof(uint16_t));
		if (vad_state == SWITCH_VAD_STATE_STOP_TALKING) {			
			switch_set_flag(context, ASRFLAG_RESULT);
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "Talking stopped, have result.\n");
			switch_vad_reset(context->vad);
			switch_clear_flag(context, ASRFLAG_READY);
		} else if (vad_state == SWITCH_VAD_STATE_START_TALKING) {
                        if(globals.log_level == 7 || globals.log_level == 5)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE , "SWITCH_VAD_STATE_START_TALKING capturing Audio.\n");
                        //if (SWITCH_STATUS_SUCCESS != start_capture(lsession,flag,lang,interim)){
                        if(globals.log_level == 7 || globals.log_level == 6)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[ GOOGLE_ASR ] : in asr_feed \n");
                          /*      status = SWITCH_STATUS_BREAK;
                        }*/
                        switch_set_flag(context, ASRFLAG_START_OF_SPEECH);
	                context->speech_time = switch_micro_time_now();
	        }
        }
	return status;
}

static switch_status_t google_asr_feed_dtmf(switch_asr_handle_t *ah, const switch_dtmf_t *dtmf, switch_asr_flag_t *flags)
{
        google_asr_t *context = (google_asr_t *) ah->private_info;        
        char buffer[100];       
        strcpy(buffer,switch_mprintf("%c",dtmf->digit));      
        strcat(context->final_dtmf_digits,buffer);       
        return SWITCH_STATUS_SUCCESS;
}


/*! function to pause recognizer */
static switch_status_t google_asr_pause(switch_asr_handle_t *ah){
	google_asr_t *context = (google_asr_t *) ah->private_info;
	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
                if(globals.log_level == 7 || globals.log_level == 3)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "google_asr_pause attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Pausing\n");
	context->flags = 0;
	return SWITCH_STATUS_SUCCESS;   
}

/*! function to resume recognizer */
static switch_status_t google_asr_resume(switch_asr_handle_t *ah){
	google_asr_t *context = (google_asr_t *) ah->private_info;
	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
                if(globals.log_level == 7 || globals.log_level == 3)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "google_asr_resume attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}        
        if(globals.log_level == 7 )
	        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Resuming\n");

	google_asr_reset(context);
	return SWITCH_STATUS_SUCCESS;
}

/*! function to read results from the ASR*/
static switch_status_t google_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags){
	google_asr_t *context = (google_asr_t *) ah->private_info;

	if (switch_test_flag(context, ASRFLAG_RETURNED_RESULT) || switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_NOTICE, "[google_asr_check_results] ASRFLAG_RETURNED_RESULT Recevied \n");
                return SWITCH_STATUS_BREAK;
	}
	if (!switch_test_flag(context, ASRFLAG_RETURNED_START_OF_SPEECH) && switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) {
               // switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_NOTICE, "[google_asr_check_results] ASRFLAG_RETURNED_START_OF_SPEECH Recevied \n");
		return SWITCH_STATUS_SUCCESS;
	}

	if ((!switch_test_flag(context, ASRFLAG_RESULT)) && (!switch_test_flag(context, ASRFLAG_NOINPUT_TIMEOUT))) {	   

           if(switch_test_flag(context, ASRFLAG_INPUT_TIMERS) && !(switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) 
                && context->no_input_timeout >= 0 && (switch_micro_time_now() - context->no_input_time) / 1000 >= context->no_input_timeout){
                        if(globals.log_level == 7 )
                                switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "NO INPUT TIMEOUT %" SWITCH_TIME_T_FMT "ms\n", (switch_micro_time_now() - context->no_input_time) / 1000);                
                        switch_set_flag(context, ASRFLAG_NOINPUT_TIMEOUT);
	                }else if (switch_test_flag(context, ASRFLAG_START_OF_SPEECH) && context->speech_timeout > 0 && (switch_micro_time_now() - context->speech_time) / 1000 >= context->speech_timeout) {
                                if(globals.log_level == 7 )
                                        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "SPEECH TIMEOUT %" SWITCH_TIME_T_FMT "ms\n", (switch_micro_time_now() - context->speech_time) / 1000);
                
                                if (switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) {
                                                switch_set_flag(context, ASRFLAG_RESULT);
                                }else{
                                        switch_set_flag(context, ASRFLAG_NOINPUT_TIMEOUT);
                                }
	        }
        }
	return switch_test_flag(context, ASRFLAG_RESULT) || switch_test_flag(context, ASRFLAG_NOINPUT_TIMEOUT) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_BREAK;   
}

/*! function to read results from the ASR */
static switch_status_t google_asr_get_results(switch_asr_handle_t *ah, char **resultstr, switch_asr_flag_t *flags)
{
	google_asr_t *context = (google_asr_t *) ah->private_info;
	switch_uuid_t uuid_t;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
        char outfile[1024] = "";
	char current_datetime[128];
        time_t t;
        struct tm* ptm;	
	char *out;
        char start_time_buffer[100];
        char end_time_buffer[100];
        char duration_time_buffer[100];
        char sample_rate_buffer[100];
        double total_time = 0;
        double start_time_parameter = 0;
        double end_time_parameter = 0;
        cJSON *root, *recog_details_record,*input,*recog_results ;        
	//char buffer[1024] = {0};
        char opbuffer[1024]={0};
        char* unique_ID = NULL;
	FILE *fptr;
        switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_stream_handle_t result = { 0 };
        switch_core_session_t *lsession = NULL;
        lsession = switch_core_session_locate(uuid);
	//switch_uuid_get(&uuid_t);
       // switch_uuid_format(uuid_str, &uuid_t);
       // switch_snprintf(outfile, sizeof(outfile), "/tmp/%s.json", uuid_str);
       // context->file = switch_core_strdup(ah->memory_pool,outfile);
	end = clock();

        start_time_parameter = ((double)(start)/(CLOCKS_PER_SEC/1000));
        sprintf(start_time_buffer,"%f ms",start_time_parameter);
        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "start_time_parameter :%s",start_time_buffer);

        end_time_parameter = ((double)(end));
        sprintf(end_time_buffer,"%f ms",end_time_parameter/(CLOCKS_PER_SEC/1000));
        ///switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "end_time_parameter :%s",end_time_buffer);

        total_time = end_time_buffer - start_time_buffer;        
        sprintf(duration_time_buffer,"%f ms",total_time);
        sprintf(sample_rate_buffer, "%d Hz", sampling_rate);
        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "total_time :%s"  ,duration_time_buffer);

        if(lsession != NULL){
                switch_channel_t *channel = switch_core_session_get_channel(lsession);                
                //if(globals.log_level == 7)
                //        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Get transcription_data:[%s]",switch_channel_get_variable(channel,"transcription_data"));
                context -> result_text = switch_channel_get_variable(channel,"transcription_data");                
                unique_ID = (char*)switch_channel_get_uuid(channel);                
        }
         
        switch_snprintf(outfile, sizeof(outfile), "/tmp/%s.json", unique_ID);
        context->file = switch_core_strdup(ah->memory_pool,outfile);
        
	t = time(NULL);
        ptm = localtime(&t);
        strftime(current_datetime, 128, "%d-%b-%Y %H:%M:%S",ptm);
        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "current date and time:[%s]",current_datetime);
        
        root = cJSON_CreateObject();

        recog_details_record = cJSON_CreateObject();        
        cJSON_AddItemToObject(recog_details_record, "datetime", cJSON_CreateString(current_datetime));
        cJSON_AddItemToObject(recog_details_record, "samplingrate", cJSON_CreateString(sample_rate_buffer));
        cJSON_AddItemToObject(recog_details_record, "language", cJSON_CreateString(language_parameter));
        cJSON_AddItemToObject(recog_details_record, "max_alternatives", cJSON_CreateString((char *)&max_alternatives_parameter));
        cJSON_AddItemToObject(root, "recog_details_record", recog_details_record);

        input = cJSON_CreateObject();
        cJSON_AddItemToObject(input, "type", cJSON_CreateString("speech"));
        cJSON_AddItemToObject(input, "start-of-input-ts", cJSON_CreateString(start_time_buffer));
        cJSON_AddItemToObject(input, "end-of-input-ts", cJSON_CreateString(end_time_buffer));        
        cJSON_AddItemToObject(input, "duration", cJSON_CreateString(duration_time_buffer));
        cJSON_AddItemToObject(input, "end-of-input-cause", cJSON_CreateString("success"));
        cJSON_AddItemToObject(root, "input", input);

        recog_results = cJSON_CreateObject();
        if(context->result_text != NULL){ 
                cJSON_AddItemToObject(recog_results, "transcription", cJSON_CreateString(context->result_text));
        }
        cJSON_AddItemToObject(recog_results, "DTMF", cJSON_CreateString(context->final_dtmf_digits));
        
        cJSON_AddItemToObject(root, "recog-results", recog_results);

        out = cJSON_Print(root);
        strcat(opbuffer,out);
	/*fptr = fopen(context->file, "w");
	fgets(opbuffer,strlen(opbuffer),stdin);
	fputs(opbuffer,fptr);        
        if(globals.log_level == 7 )
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Transcription results added into [%s]",context->file);
	 free(out);
        cJSON_Delete(root);
	fclose(fptr);*/	
        SWITCH_STANDARD_STREAM(result);
	if (switch_test_flag(context, ASRFLAG_RETURNED_RESULT) || switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		return SWITCH_STATUS_FALSE;
	}

         if(globals.log_level == 7 ){
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Transcription result:[%s]",opbuffer);
                //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Get google_asr_get_results:[%s]",*resultstr);
                //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Get DTMF Results[c] google_asr_get_results:[%c]",context->dtmf_digits);
                //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Get DTMF Results[d] google_asr_get_results FinalDigits:[%s]",context->final_dtmf_digits);
        }

	if (switch_test_flag(context, ASRFLAG_RESULT)) {
                //*resultstr = switch_mprintf("%s",context->result_text); 
                *resultstr = switch_mprintf("%s",opbuffer); 
		//*resultstr = switch_mprintf("{\"grammar\": \"%s\", \"text\": \"%s\", \"confidence\": %f}", context->grammar, context->result_text, context->result_confidence);
		if(globals.log_level == 7)
                switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG,"[google_asr_get_results] ASRFLAG_RESULT: %s\n", *resultstr);
		status = SWITCH_STATUS_SUCCESS;
	} else if (switch_test_flag(context, ASRFLAG_NOINPUT_TIMEOUT)) {
                if(globals.log_level == 7)
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "[google_asr_get_results] ASRFLAG_NOINPUT_TIMEOUT\n");
                *resultstr = switch_mprintf("%s",opbuffer);  
		//*resultstr = switch_mprintf("{\"grammar\": \"%s\", \"text\": \"\", \"confidence\": 0, \"error\": \"no_input\"}", context->grammar);
		status = SWITCH_STATUS_SUCCESS;
	} else if (!switch_test_flag(context, ASRFLAG_RETURNED_START_OF_SPEECH) && switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) {
		switch_set_flag(context, ASRFLAG_RETURNED_START_OF_SPEECH);
                if(globals.log_level == 7)
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "[ Received ASRFLAG_RETURNED_START_OF_SPEECH],[ASRFLAG_START_OF_SPEECH] \n");
		status = SWITCH_STATUS_BREAK;
	} else {
                if(globals.log_level == 7 || globals.log_level == 3)
		        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_ERROR, "Unexpected call to asr_get_results - no results to return!\n");
		
                status = SWITCH_STATUS_FALSE;
	}

        if(context->file == NULL){
                switch_uuid_get(&uuid_t);
                switch_uuid_format(uuid_str, &uuid_t);
                switch_snprintf(outfile, sizeof(outfile), "/tmp/%s.json", uuid_str);
                context->file = switch_core_strdup(ah->memory_pool,outfile);
        }
        fptr = fopen(context->file, "w");
	fgets(opbuffer,strlen(opbuffer),stdin);
	fputs(opbuffer,fptr);        
        if(globals.log_level == 7 )
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Transcription results added into [%s]",context->file);
	 free(out);
        cJSON_Delete(root);
	fclose(fptr);
	if (status == SWITCH_STATUS_SUCCESS) {
		switch_set_flag(context, ASRFLAG_RETURNED_RESULT);
		switch_clear_flag(context, ASRFLAG_READY);
	}





        switch_core_session_rwunlock(lsession);
	return status;
  

}

/*! function to start input timeouts */
static switch_status_t google_asr_start_input_timers(switch_asr_handle_t *ah){
	google_asr_t *context = (google_asr_t *) ah->private_info;
	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
                if(globals.log_level == 7 || globals.log_level == 3)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "google_asr_start_input_timers attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}
	
	if (!switch_test_flag(context, ASRFLAG_INPUT_TIMERS)) {
                if(globals.log_level == 7)
                switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "google_asr_start_input_timers\n");
		switch_set_flag(context, ASRFLAG_INPUT_TIMERS);
		context->no_input_time = switch_micro_time_now();
	} else {
                if(globals.log_level == 7 || globals.log_level == 6)
		        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "Input timers already started\n");
	}
	return SWITCH_STATUS_SUCCESS;
}

/*! function to load a grammar to the asr interface */
static switch_status_t google_asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *name){
        google_asr_t *context = (google_asr_t *)ah->private_info;
	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
                if(globals.log_level == 7 || globals.log_level == 3)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[google_asr_load_grammar] SWITCH_ASR_FLAG_CLOSED \n");
		return SWITCH_STATUS_FALSE;
	}
        if(globals.log_level == 7 || globals.log_level == 6)
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "[google_asr_load_grammar] %s\n", grammar);
	context->grammar = switch_core_strdup(ah->memory_pool, grammar);
	return SWITCH_STATUS_SUCCESS;
}

/*! function to unload a grammar to the asr interface */
static switch_status_t google_asr_unload_grammar(switch_asr_handle_t *ah, const char *name)
{
        if(globals.log_level == 7 || globals.log_level == 6)
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[google_asr_unload_grammar] \n");
	
        return SWITCH_STATUS_SUCCESS;
}

/*! set text parameter */
static void google_asr_text_param(switch_asr_handle_t *ah, char *param, const char *val)
{
	google_asr_t *context = (google_asr_t *) ah->private_info;

        if(globals.log_level == 7 || globals.log_level == 6)
        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "[google_asr_text_param] invoked \n");

	if (!zstr(param) && !zstr(val)) {
		int nval = atoi(val);
		double fval = atof(val);

		if (!strcasecmp("no-input-timeout", param) && switch_is_number(val)) {
			context->no_input_timeout = nval;
                        if(globals.log_level == 7 )
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "no-input-timeout = %d\n", context->no_input_timeout);

		} else if (!strcasecmp("speech-timeout", param) && switch_is_number(val)) {
			context->speech_timeout = nval;
                        if(globals.log_level == 7)
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "speech-timeout = %d\n", context->speech_timeout);

		} else if (!strcasecmp("start-input-timers", param)) {
			context->start_input_timers = switch_true(val);
			if (context->start_input_timers) {
				switch_set_flag(context, ASRFLAG_INPUT_TIMERS);
			} else {
				switch_clear_flag(context, ASRFLAG_INPUT_TIMERS);
			}
                        if(globals.log_level == 7 )
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "start-input-timers = %d\n", context->start_input_timers);

		} else if (!strcasecmp("vad-mode", param)) {
                        if(globals.log_level == 7)
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "vad-mode = %s\n", val);

			if (context->vad) switch_vad_set_mode(context->vad, nval);
		} else if (!strcasecmp("vad-voice-ms", param) && nval > 0) {
			context->voice_ms = nval;
			switch_vad_set_param(context->vad, "voice_ms", nval);
		} else if (!strcasecmp("vad-silence-ms", param) && nval > 0) {
			context->silence_ms = nval;
			switch_vad_set_param(context->vad, "silence_ms", nval);
		} else if (!strcasecmp("vad-thresh", param) && nval > 0) {
			context->thresh = nval;
			switch_vad_set_param(context->vad, "thresh", nval);
		} else if (!strcasecmp("channel-uuid", param)) {
			context->channel_uuid = switch_core_strdup(ah->memory_pool, val);
                        if(globals.log_level == 7 )
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "channel-uuid = %s\n", val);
		} else if (!strcasecmp("result", param)) {
			context->result_text = switch_core_strdup(ah->memory_pool, val);
                        if(globals.log_level == 7 )
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "result = %s\n", val);
		} else if (!strcasecmp("confidence", param) && fval >= 0.0) {
			context->result_confidence = fval;
                        if(globals.log_level == 7 )
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "confidence = %f\n", fval);
		}
	}
}

/*! set numeric parameter */
static void google_asr_numeric_param(switch_asr_handle_t *ah, char *param, int val)
{
}

/*! set float parameter */
static void google_asr_float_param(switch_asr_handle_t *ah, char *param, double val)
{
}

SWITCH_STANDARD_APP(GOOGLE_ASR)
{
        if(session){
                user_channel = switch_core_session_get_channel(session);
                uuid = switch_channel_get_uuid(user_channel);
		google_lang = (char *)(switch_channel_get_variable(user_channel,"mod_google_lang"));                

        }else{
           if(globals.log_level == 7 || globals.log_level == 2)
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "[ GOOGLE_ASR ] : session is not created\n");           
           return;
        }      
       
}



SWITCH_MODULE_LOAD_FUNCTION(mod_google_asr_load)
{
	switch_status_t status;
	switch_asr_interface_t *asr_interface;
	switch_application_interface_t *app_interface;

	if ((status = google_load_config()) != SWITCH_STATUS_SUCCESS){
                return status;
        }

	if (switch_event_reserve_subclass(TRANSCRIBE_EVENT_RESULTS) != SWITCH_STATUS_SUCCESS) {
                if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", TRANSCRIBE_EVENT_RESULTS);

                return SWITCH_STATUS_TERM;
        }

        if (switch_event_reserve_subclass(TRANSCRIBE_EVENT_END_OF_UTTERANCE) != SWITCH_STATUS_SUCCESS) {
                if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", TRANSCRIBE_EVENT_END_OF_UTTERANCE);
                
                return SWITCH_STATUS_TERM;
        }

        if (switch_event_reserve_subclass(TRANSCRIBE_EVENT_START_OF_TRANSCRIPT) != SWITCH_STATUS_SUCCESS) {
                if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", TRANSCRIBE_EVENT_START_OF_TRANSCRIPT);
                
                return SWITCH_STATUS_TERM;
        }

        if (switch_event_reserve_subclass(TRANSCRIBE_EVENT_END_OF_TRANSCRIPT) != SWITCH_STATUS_SUCCESS) {
                if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", TRANSCRIBE_EVENT_END_OF_TRANSCRIPT);
                
                return SWITCH_STATUS_TERM;
        }

        if (switch_event_reserve_subclass(TRANSCRIBE_EVENT_NO_AUDIO_DETECTED) != SWITCH_STATUS_SUCCESS) {
                if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", TRANSCRIBE_EVENT_NO_AUDIO_DETECTED);
                
                return SWITCH_STATUS_TERM;
        }

        if (switch_event_reserve_subclass(TRANSCRIBE_EVENT_MAX_DURATION_EXCEEDED) != SWITCH_STATUS_SUCCESS) {
                if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", TRANSCRIBE_EVENT_MAX_DURATION_EXCEEDED);
                
                return SWITCH_STATUS_TERM;
        }

        if (switch_event_reserve_subclass(TRANSCRIBE_EVENT_PLAY_INTERRUPT) != SWITCH_STATUS_SUCCESS) {
                if(globals.log_level == 7 || globals.log_level == 3)
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", TRANSCRIBE_EVENT_PLAY_INTERRUPT);
                
                return SWITCH_STATUS_TERM;
        }


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	asr_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ASR_INTERFACE);
	asr_interface->interface_name = "google_asr";
	asr_interface->asr_open = google_asr_open;
	asr_interface->asr_load_grammar = google_asr_load_grammar;
	asr_interface->asr_unload_grammar = google_asr_unload_grammar;
	asr_interface->asr_close = google_asr_close;
	asr_interface->asr_feed = google_asr_feed;
        asr_interface->asr_feed_dtmf = google_asr_feed_dtmf;
	asr_interface->asr_resume = google_asr_resume;
	asr_interface->asr_pause = google_asr_pause;
	asr_interface->asr_check_results = google_asr_check_results;
	asr_interface->asr_get_results = google_asr_get_results;
	asr_interface->asr_start_input_timers = google_asr_start_input_timers;
	asr_interface->asr_text_param = google_asr_text_param;
	asr_interface->asr_numeric_param = google_asr_numeric_param;
	asr_interface->asr_float_param = google_asr_float_param;

	SWITCH_ADD_APP(app_interface,"GOOGLE_ASR","Welcome to GOOGLE_ASR application","",GOOGLE_ASR,"",SAF_NONE);
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_google_asr_shutdown)
{
	google_speech_cleanup();
        switch_event_free_subclass(TRANSCRIBE_EVENT_RESULTS);
        switch_event_free_subclass(TRANSCRIBE_EVENT_END_OF_UTTERANCE);
        switch_event_free_subclass(TRANSCRIBE_EVENT_START_OF_TRANSCRIPT);
        switch_event_free_subclass(TRANSCRIBE_EVENT_END_OF_TRANSCRIPT);
        switch_event_free_subclass(TRANSCRIBE_EVENT_NO_AUDIO_DETECTED);
        switch_event_free_subclass(TRANSCRIBE_EVENT_MAX_DURATION_EXCEEDED);
        switch_event_free_subclass(TRANSCRIBE_EVENT_END_OF_UTTERANCE);
        switch_event_free_subclass(TRANSCRIBE_EVENT_PLAY_INTERRUPT);
	return SWITCH_STATUS_UNLOAD;
}


