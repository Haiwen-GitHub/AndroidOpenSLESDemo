#include <jni.h>
#include <string>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "XLog.h"
#include <pthread.h>

#define LOG(...) __android_log_print(ANDROID_LOG_DEBUG,"AudioDemo-JNI",__VA_ARGS__)

#define SAMPLERATE 44100
#define CHANNELS 1
#define PERIOD_TIME 20 //ms
#define FRAME_SIZE SAMPLERATE*PERIOD_TIME/1000
#define BUFFER_SIZE FRAME_SIZE*CHANNELS
#define TEST_CAPTURE_FILE_PATH "/sdcard/audio.pcm"

#define CONV16BIT 32768
#define CONVMYFLT (1./32768.)
typedef struct threadLock_ {
    pthread_mutex_t m;
    pthread_cond_t  c;
    unsigned char   s;
} threadLock;

typedef struct opensl_stream {

    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;

    // output mix interfaces
    SLObjectItf outputMixObject;

    // buffer queue player interfaces
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLEffectSendItf bqPlayerEffectSend;

    // recorder interfaces
    SLObjectItf recorderObject;
    SLRecordItf recorderRecord;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;

    // buffer indexes
    int currentInputIndex;
    int currentOutputIndex;

    // current buffer half (0, 1)
    int currentOutputBuffer;
    int currentInputBuffer;

    // buffers
    short *outputBuffer[2];
    short *inputBuffer[2];

    // size of buffers
    int outBufSamples;
    int inBufSamples;

    // locks
    void*  inlock;
    void*  outlock;

    double time;
    int inchannels;
    int outchannels;
    int sr;

} OPENSL_STREAM;

static volatile int g_loop_exit = 0;


void notifyThreadLock(void *lock)
{
    threadLock *p;
    p = (threadLock*) lock;
    pthread_mutex_lock(&(p->m));
    p->s = (unsigned char)1;
    pthread_cond_signal(&(p->c));
    pthread_mutex_unlock(&(p->m));
}

//----------------------------------------------------------------------
// thread Locks
// to ensure synchronisation between callbacks and processing code
void* createThreadLock(void)
{
    threadLock  *p;
    p = (threadLock*) malloc(sizeof(threadLock));
    if (p == NULL)
        return NULL;
    memset(p, 0, sizeof(threadLock));
    if (pthread_mutex_init(&(p->m), (pthread_mutexattr_t*) NULL) != 0) {
        free((void*) p);
        return NULL;
    }
    if (pthread_cond_init(&(p->c), (pthread_condattr_t*) NULL) != 0) {
        pthread_mutex_destroy(&(p->m));
        free((void*) p);
        return NULL;
    }
    p->s = (unsigned char)1;

    return p;
}

int waitThreadLock(void *lock)
{
    LOG("waitThreadLock 1\n");
    threadLock  *p;
    int retval = 0;
    p = (threadLock*)lock;
    LOG("waitThreadLock 2\n");
    pthread_mutex_lock(&(p->m));
    LOG("waitThreadLock 3\n");
    while (!p->s) {
        pthread_cond_wait(&(p->c), &(p->m));
    }
    LOG("waitThreadLock 4\n");
    p->s = (unsigned char)0;
    pthread_mutex_unlock(&(p->m));
    LOG("waitThreadLock 5\n");
}



void destroyThreadLock(void *lock)
{
    threadLock  *p;
    p = (threadLock*) lock;
    if (p == NULL)
        return;
    notifyThreadLock(p);
    pthread_cond_destroy(&(p->c));
    pthread_mutex_destroy(&(p->m));
    free(p);
}
static SLresult openSLCreateEngine(OPENSL_STREAM *p)
{
    SLresult result;
    // create engine
    result = slCreateEngine(&(p->engineObject), 0, NULL, 0, NULL, NULL);
    if(result != SL_RESULT_SUCCESS) goto  engine_end;

    // realize the engine
    result = (*p->engineObject)->Realize(p->engineObject, SL_BOOLEAN_FALSE);
    if(result != SL_RESULT_SUCCESS) goto engine_end;

    // get the engine interface, which is needed in order to create other objects
    result = (*p->engineObject)->GetInterface(p->engineObject, SL_IID_ENGINE, &(p->engineEngine));
    if(result != SL_RESULT_SUCCESS) goto  engine_end;

    engine_end:
    return result;
}
// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    OPENSL_STREAM *p = (OPENSL_STREAM *) context;
    notifyThreadLock(p->outlock);
}
// opens the OpenSL ES device for output
static SLresult openSLPlayOpen(OPENSL_STREAM *p)
{
    SLresult result;
    SLuint32 sr = p->sr;
    SLuint32  channels = p->outchannels;

    if(channels) {
        // configure audio source
        SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

        switch(sr){
            case 8000:
                sr = SL_SAMPLINGRATE_8;
                break;
            case 11025:
                sr = SL_SAMPLINGRATE_11_025;
                break;
            case 16000:
                sr = SL_SAMPLINGRATE_16;
                break;
            case 22050:
                sr = SL_SAMPLINGRATE_22_05;
                break;
            case 24000:
                sr = SL_SAMPLINGRATE_24;
                break;
            case 32000:
                sr = SL_SAMPLINGRATE_32;
                break;
            case 44100:
                sr = SL_SAMPLINGRATE_44_1;
                break;
            case 48000:
                sr = SL_SAMPLINGRATE_48;
                break;
            case 64000:
                sr = SL_SAMPLINGRATE_64;
                break;
            case 88200:
                sr = SL_SAMPLINGRATE_88_2;
                break;
            case 96000:
                sr = SL_SAMPLINGRATE_96;
                break;
            case 192000:
                sr = SL_SAMPLINGRATE_192;
                break;
            default:
                break;

                return -1;
        }

        const SLInterfaceID ids[] = {SL_IID_VOLUME};
        const SLboolean req[] = {SL_BOOLEAN_FALSE};
        result = (*p->engineEngine)->CreateOutputMix(p->engineEngine, &(p->outputMixObject), 1, ids, req);
        if(result != SL_RESULT_SUCCESS) return result;

        // realize the output mix
        result = (*p->outputMixObject)->Realize(p->outputMixObject, SL_BOOLEAN_FALSE);

        SLuint32   speakers;
        if(channels > 1)
            speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
        else speakers = SL_SPEAKER_FRONT_CENTER;


        SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,channels, sr,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       speakers, SL_BYTEORDER_LITTLEENDIAN};

        SLDataSource audioSrc = {&loc_bufq, &format_pcm};

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, p->outputMixObject};
        SLDataSink audioSnk = {&loc_outmix, NULL};

        // create audio player
        const SLInterfaceID ids1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req1[] = {SL_BOOLEAN_TRUE};
        result = (*p->engineEngine)->CreateAudioPlayer(p->engineEngine, &(p->bqPlayerObject), &audioSrc, &audioSnk,
                                                       1, ids1, req1);
        if(result != SL_RESULT_SUCCESS) return result;

        // realize the player
        result = (*p->bqPlayerObject)->Realize(p->bqPlayerObject, SL_BOOLEAN_FALSE);
        if(result != SL_RESULT_SUCCESS) return result;

        // get the play interface
        result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_PLAY, &(p->bqPlayerPlay));
        if(result != SL_RESULT_SUCCESS) return result;

        // get the buffer queue interface
        result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                    &(p->bqPlayerBufferQueue));
        if(result != SL_RESULT_SUCCESS) return result;

        // register callback on the buffer queue
        result = (*p->bqPlayerBufferQueue)->RegisterCallback(p->bqPlayerBufferQueue, bqPlayerCallback, p);
        if(result != SL_RESULT_SUCCESS) return result;

        // set the player's state to playing
        result = (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);

        return result;

    }

    return SL_RESULT_SUCCESS;
}
// this callback handler is called every time a buffer finishes recording
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    OPENSL_STREAM *p = (OPENSL_STREAM *) context;
    notifyThreadLock(p->inlock);
}
// Open the OpenSL ES device for input
static SLresult openSLRecOpen(OPENSL_STREAM *p)
{
    SLresult result;
    SLuint32 sr = p->sr;
    SLuint32 channels = p->inchannels;

    if(channels) {
        switch(sr) {
            case 8000:
                sr = SL_SAMPLINGRATE_8;
                break;
            case 11025:
                sr = SL_SAMPLINGRATE_11_025;
                break;
            case 16000:
                sr = SL_SAMPLINGRATE_16;
                break;
            case 22050:
                sr = SL_SAMPLINGRATE_22_05;
                break;
            case 24000:
                sr = SL_SAMPLINGRATE_24;
                break;
            case 32000:
                sr = SL_SAMPLINGRATE_32;
                break;
            case 44100:
                sr = SL_SAMPLINGRATE_44_1;
                break;
            case 48000:
                sr = SL_SAMPLINGRATE_48;
                break;
            case 64000:
                sr = SL_SAMPLINGRATE_64;
                break;
            case 88200:
                sr = SL_SAMPLINGRATE_88_2;
                break;
            case 96000:
                sr = SL_SAMPLINGRATE_96;
                break;
            case 192000:
                sr = SL_SAMPLINGRATE_192;
                break;
            default:
                break;

                return -1;
        }

        // configure audio source
        SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                          SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
        SLDataSource audioSrc = {&loc_dev, NULL};

        // configure audio sink
        SLuint32 speakers;
        if(channels > 1)
            speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
        else speakers = SL_SPEAKER_FRONT_CENTER;

        SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
        SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, channels, sr,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       speakers, SL_BYTEORDER_LITTLEENDIAN};
        SLDataSink audioSnk = {&loc_bq, &format_pcm};

        // create audio recorder
        // (requires the RECORD_AUDIO permission)
        const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        result = (*p->engineEngine)->CreateAudioRecorder(p->engineEngine, &(p->recorderObject), &audioSrc,
                                                         &audioSnk, 1, id, req);
        if (SL_RESULT_SUCCESS != result) goto end_recopen;

        // realize the audio recorder
        result = (*p->recorderObject)->Realize(p->recorderObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) goto end_recopen;

        // get the record interface
        result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_RECORD, &(p->recorderRecord));
        if (SL_RESULT_SUCCESS != result) goto end_recopen;

        // get the buffer queue interface
        result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                    &(p->recorderBufferQueue));
        if (SL_RESULT_SUCCESS != result) goto end_recopen;

        // register callback on the buffer queue
        result = (*p->recorderBufferQueue)->RegisterCallback(p->recorderBufferQueue, bqRecorderCallback,
                                                             p);
        if (SL_RESULT_SUCCESS != result) goto end_recopen;
        result = (*p->recorderRecord)->SetRecordState(p->recorderRecord, SL_RECORDSTATE_RECORDING);

        end_recopen:
        return result;
    }
    else
        return SL_RESULT_SUCCESS;
}

// close the OpenSL IO and destroy the audio engine
static void openSLDestroyEngine(OPENSL_STREAM *p)
{
    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (p->bqPlayerObject != NULL) {
        (*p->bqPlayerObject)->Destroy(p->bqPlayerObject);
        p->bqPlayerObject = NULL;
        p->bqPlayerPlay = NULL;
        p->bqPlayerBufferQueue = NULL;
        p->bqPlayerEffectSend = NULL;
    }

    // destroy audio recorder object, and invalidate all associated interfaces
    if (p->recorderObject != NULL) {
        (*p->recorderObject)->Destroy(p->recorderObject);
        p->recorderObject = NULL;
        p->recorderRecord = NULL;
        p->recorderBufferQueue = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (p->outputMixObject != NULL) {
        (*p->outputMixObject)->Destroy(p->outputMixObject);
        p->outputMixObject = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (p->engineObject != NULL) {
        (*p->engineObject)->Destroy(p->engineObject);
        p->engineObject = NULL;
        p->engineEngine = NULL;
    }
}
// close the android audio device
void android_CloseAudioDevice(OPENSL_STREAM *p)
{
    if (p == NULL)
        return;

    openSLDestroyEngine(p);

    if (p->inlock != NULL) {
        notifyThreadLock(p->inlock);
        destroyThreadLock(p->inlock);
        p->inlock = NULL;
    }

    if (p->outlock != NULL) {
        notifyThreadLock(p->outlock);
        destroyThreadLock(p->outlock);
        p->inlock = NULL;
    }

    if (p->outputBuffer[0] != NULL) {
        free(p->outputBuffer[0]);
        p->outputBuffer[0] = NULL;
    }

    if (p->outputBuffer[1] != NULL) {
        free(p->outputBuffer[1]);
        p->outputBuffer[1] = NULL;
    }

    if (p->inputBuffer[0] != NULL) {
        free(p->inputBuffer[0]);
        p->inputBuffer[0] = NULL;
    }

    if (p->inputBuffer[1] != NULL) {
        free(p->inputBuffer[1]);
        p->inputBuffer[1] = NULL;
    }

    free(p);
}
// open the android audio device for input and/or output
OPENSL_STREAM *android_OpenAudioDevice(int sr, int inchannels, int outchannels, int bufferframes)
{
    OPENSL_STREAM *p;
    p = (OPENSL_STREAM *) calloc(sizeof(OPENSL_STREAM),1);

    p->inchannels = inchannels;
    p->outchannels = outchannels;
    p->sr = sr;
    p->inlock = createThreadLock();
    p->outlock = createThreadLock();

    if((p->outBufSamples = bufferframes*outchannels) != 0) {
        if((p->outputBuffer[0] = (short *) calloc(p->outBufSamples, sizeof(short))) == NULL ||
           (p->outputBuffer[1] = (short *) calloc(p->outBufSamples, sizeof(short))) == NULL) {
            android_CloseAudioDevice(p);
            return NULL;
        }
    }

    if((p->inBufSamples = bufferframes*inchannels) != 0){
        if((p->inputBuffer[0] = (short *) calloc(p->inBufSamples, sizeof(short))) == NULL ||
           (p->inputBuffer[1] = (short *) calloc(p->inBufSamples, sizeof(short))) == NULL){
            android_CloseAudioDevice(p);
            return NULL;
        }
    }

    p->currentInputIndex = 0;
    p->currentOutputBuffer  = 0;
    p->currentInputIndex = p->inBufSamples;
    p->currentInputBuffer = 0;

    if(openSLCreateEngine(p) != SL_RESULT_SUCCESS) {
        android_CloseAudioDevice(p);
        return NULL;
    }

    if(openSLRecOpen(p) != SL_RESULT_SUCCESS) {
        android_CloseAudioDevice(p);
        return NULL;
    }

    if(openSLPlayOpen(p) != SL_RESULT_SUCCESS) {
        android_CloseAudioDevice(p);
        return NULL;
    }

    notifyThreadLock(p->outlock);
    notifyThreadLock(p->inlock);

    p->time = 0.;
    return p;
}



// returns timestamp of the processed stream
double android_GetTimestamp(OPENSL_STREAM *p)
{
    return p->time;
}



// gets a buffer of size samples from the device
int android_AudioIn(OPENSL_STREAM *p,short *buffer,int size)
{
    LOG("android_AudioIn \n");
    short *inBuffer;
    int i, bufsamps = p->inBufSamples, index = p->currentInputIndex;
    if(p == NULL || bufsamps ==  0) return 0;

    inBuffer = p->inputBuffer[p->currentInputBuffer];
    for(i=0; i < size; i++){
        if (index >= bufsamps) {
            waitThreadLock(p->inlock);
            (*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue,inBuffer,bufsamps*sizeof(short));
            p->currentInputBuffer = (p->currentInputBuffer ? 0 : 1);
            index = 0;
            inBuffer = p->inputBuffer[p->currentInputBuffer];
        }
        buffer[i] = (short)inBuffer[index++];
    }
    p->currentInputIndex = index;
    if(p->outchannels == 0) p->time += (double) size/(p->sr*p->inchannels);
    return i;
}



// puts a buffer of size samples to the device
int android_AudioOut(OPENSL_STREAM *p, short *buffer,int size)
{
    LOG("android_AudioOut \n");
    short *outBuffer;
    int i, bufsamps = p->outBufSamples, index = p->currentOutputIndex;
    if(p == NULL  || bufsamps ==  0)  return 0;
    outBuffer = p->outputBuffer[p->currentOutputBuffer];

    for(i=0; i < size; i++){
        outBuffer[index++] = (short)(buffer[i]);
        if (index >= p->outBufSamples) {
            waitThreadLock(p->outlock);
            (*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue,
                                               outBuffer,bufsamps*sizeof(short));
            p->currentOutputBuffer = (p->currentOutputBuffer ?  0 : 1);
            index = 0;
            outBuffer = p->outputBuffer[p->currentOutputBuffer];
        }
    }
    p->currentOutputIndex = index;
    p->time += (double) size/(p->sr*p->outchannels);
    return i;
}






/*
 * Class:     com_jhuster_audiodemo_tester_NativeAudioTester
 * Method:    nativeStartCapture
 * Signature: ()Z
 */
extern "C" JNIEXPORT jboolean JNICALL Java_com_jhuster_audiodemo_tester_NativeAudioTester_nativeStartCapture(JNIEnv *env, jobject thiz)
{
    FILE * fp = fopen(TEST_CAPTURE_FILE_PATH, "wb");
    if( fp == NULL ) {
        LOG("cannot open file (%s)\n", TEST_CAPTURE_FILE_PATH);
        return -1;
    }

    OPENSL_STREAM* stream = android_OpenAudioDevice(SAMPLERATE, CHANNELS, CHANNELS, FRAME_SIZE);
    if (stream == NULL) {
        fclose(fp);
        LOG("failed to open audio device ! \n");
        return JNI_FALSE;
    }

    int samples;
    short buffer[BUFFER_SIZE];
    g_loop_exit = 0;
    while (!g_loop_exit) {
        samples = android_AudioIn(stream, buffer, BUFFER_SIZE);
        if (samples < 0) {
            LOG("android_AudioIn failed !\n");
            break;
        }
        if (fwrite((unsigned char *)buffer, samples*sizeof(short), 1, fp) != 1) {
            LOG("failed to save captured data !\n ");
            break;
        }
        LOG("capture %d samples !\n", samples);
    }

    android_CloseAudioDevice(stream);
    fclose(fp);

    LOG("nativeStartCapture completed !");

    return JNI_TRUE;
}

/*
 * Class:     com_jhuster_audiodemo_tester_NativeAudioTester
 * Method:    nativeStopCapture
 * Signature: ()Z
 */
extern "C" JNIEXPORT jboolean JNICALL Java_com_jhuster_audiodemo_tester_NativeAudioTester_nativeStopCapture(JNIEnv *env, jobject thiz)
{
    g_loop_exit = 1;
    return JNI_TRUE;
}

/*
 * Class:     com_jhuster_audiodemo_tester_NativeAudioTester
 * Method:    nativeStartPlayback
 * Signature: ()Z
 */
extern "C" JNIEXPORT jboolean JNICALL Java_com_jhuster_audiodemo_tester_NativeAudioTester_nativeStartPlayback(JNIEnv *env, jobject thiz)
{
    FILE * fp = fopen(TEST_CAPTURE_FILE_PATH, "rb");
    if( fp == NULL ) {
        LOG("cannot open file (%s) !\n",TEST_CAPTURE_FILE_PATH);
        return -1;
    }

    OPENSL_STREAM* stream = android_OpenAudioDevice(SAMPLERATE, CHANNELS, CHANNELS, FRAME_SIZE);
    if (stream == NULL) {
        fclose(fp);
        LOG("failed to open audio device ! \n");
        return JNI_FALSE;
    }

    int samples;
    short buffer[BUFFER_SIZE];
    g_loop_exit = 0;
    while (!g_loop_exit && !feof(fp)) {
        if (fread((unsigned char *)buffer, BUFFER_SIZE*2, 1, fp) != 1) {
            LOG("failed to read data \n ");
            break;
        }
        samples = android_AudioOut(stream, buffer, BUFFER_SIZE);
        if (samples < 0) {
            LOG("android_AudioOut failed !\n");
        }
        LOG("playback %d samples !\n", samples);
    }

    android_CloseAudioDevice(stream);
    fclose(fp);

    LOG("nativeStartPlayback completed !");

    return JNI_TRUE;
}

/*
 * Class:     com_jhuster_audiodemo_tester_NativeAudioTester
 * Method:    nativeStopPlayback
 * Signature: ()Z
 */
extern "C" JNIEXPORT jboolean JNICALL Java_com_jhuster_audiodemo_tester_NativeAudioTester_nativeStopPlayback(JNIEnv *env, jobject thiz)
{
    g_loop_exit = 1;
    return JNI_TRUE;
}







