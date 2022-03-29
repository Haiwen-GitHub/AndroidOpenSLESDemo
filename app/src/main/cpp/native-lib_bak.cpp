#include <jni.h>
#include <string>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "XLog.h"
static SLObjectItf engienSL = NULL;
SLEngineItf CreateSL()
{
    SLresult re;
    SLEngineItf en;
    re = slCreateEngine(&engienSL,0,0,0,0,0);
    if(re != SL_RESULT_SUCCESS) return NULL;
    re = (*engienSL)->Realize(engienSL,SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS) return NULL;
    re = (*engienSL)->GetInterface(engienSL,SL_IID_ENGINE,&en);
    if(re != SL_RESULT_SUCCESS) return NULL;
    return en;
}

void PcmCall(SLAndroidSimpleBufferQueueItf bf,void *contex)
{
    XLOGD("PcmCall");
    static FILE *fp = NULL;
    static char *buf = NULL;
    if(!buf){
        buf = new char[1024*1024];
    }
    if(!fp)
    {
        fp = fopen("/sdcard/test.pcm","rb"); //b 二进制打开
    }
    if(!fp) return;
    if(feof(fp) == 0){
        int len = fread(buf,1,1024,fp);
        if(len > 0){
            (*bf)->Enqueue(bf,buf,len);
        }
    }

}

extern "C" JNIEXPORT jstring JNICALL
Java_com_pengtgimust_androidopenslesdemo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    //创建引擎
    SLEngineItf eng = CreateSL();
    if(eng)
    {
        XLOGE("CreateSL Success! ");
    } else
    {
        XLOGE("CreateSL failed! ");
    }

    //2 创建混音器
    SLObjectItf mix = NULL;
    SLresult re = 0;
    re = (*eng)->CreateOutputMix(eng,&mix,0,0,0);
    if(re != SL_RESULT_SUCCESS)
    {
        XLOGD("CreateOutputMix failed! ");
    }
    (*mix)->Realize(mix,SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS)
    {
        XLOGD("(*mix)->Realize failed! ");
    }
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX,mix};
    SLDataSink audioSink = {&outputMix,0};

    //3 配置音频信息
    //缓冲队列
    SLDataLocator_AndroidSimpleBufferQueue que = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,10};
    //音频格式
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,
            2,//声道数
            SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN //字节序，小端
    };

    SLDataSource ds = {&que,&pcm};

    //4 创建播放器
    SLObjectItf player = NULL;
    SLPlayItf iplayer = NULL;
    SLAndroidSimpleBufferQueueItf pcmQue = NULL;
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};
    re = (*eng)->CreateAudioPlayer(eng,&player,&ds,&audioSink, sizeof(ids)/ sizeof(SLInterfaceID),ids,req);
    if(re != SL_RESULT_SUCCESS)
    {
        XLOGD("CreateAudioPlayer failed! ");
    } else{
        XLOGD("CreateAudioPlayer success! ");
    }
    (*player)->Realize(player,SL_BOOLEAN_FALSE);
    //获取player接口
    (*player)->GetInterface(player,SL_IID_PLAY,&iplayer);
    if(re != SL_RESULT_SUCCESS)
    {
        XLOGD("GetInterface SL_IID_PLAY failed! ");
    }
    re = (*player)->GetInterface(player,SL_IID_BUFFERQUEUE,&pcmQue);
    if(re != SL_RESULT_SUCCESS)
    {
        XLOGD("GetInterface SL_IID_BUFFERQUEUE failed! ");
    } else{
        XLOGD("GetInterface SL_IID_BUFFERQUEUE success! ");
    }

    //设置回调函数，播放队列空调用
    (*pcmQue)->RegisterCallback(pcmQue,PcmCall,0);
    //设置为播放状态
    (*iplayer)->SetPlayState(iplayer,SL_PLAYSTATE_PLAYING);

    //启动队列回调
    (*pcmQue)->Enqueue(pcmQue,"",1);

    return env->NewStringUTF(hello.c_str());
}
