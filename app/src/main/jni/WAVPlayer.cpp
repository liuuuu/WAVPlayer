//
// Created by liuuuu on 2018/7/9.
//
#include "com_liuuuu_wavplayer_MainActivity.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
#include <wavlib.h>
}

static const char *JAVA_LANG_IOEXCEPTION = "java/lang/IOException";
static const char *JAVA_LANG_OUTOFMEMORYERROR = "java/lang/OutOfMemoryError";

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

struct PlayerContext {
    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    SLObjectItf outputMixObject;
    SLObjectItf audioPlayerObject;
    SLAndroidSimpleBufferQueueItf audioPlayerBufferQueue;
    SLPlayItf audioPlayerPlay;
    WAV wav;

    unsigned char *buffer;
    size_t bufferSize;

    PlayerContext() : engineObject(0), engineEngine(0), outputMixObject(0), audioPlayerObject(0),
                      audioPlayerBufferQueue(0), audioPlayerPlay(0), wav(0), bufferSize(0) {}
};

/**
 * 从给定的类和消息抛出异常
 * @param env
 * @param className
 * @param message
 */
static void ThrowException(JNIEnv *env, const char *className, const char *message) {
    //  获取异常类
    jclass clazz = env->FindClass(className);

    // 如果异常类被发现
    if (0 != clazz) {
        // 抛出异常
        env->ThrowNew(clazz, message);
        // 释放引用
        env->DeleteLocalRef(clazz);
    }
}

/**
 * 打开给定的WAVE文件
 * @param env
 * @param fileName
 * @return
 */
static WAV OpenWaveFile(JNIEnv *env, jstring fileName) {
    WAVError error = WAV_SUCCESS;
    WAV wav = 0;

    // 以字符串的形式获取文件名
    const char *cFileName = env->GetStringUTFChars(fileName, NULL);
    if (0 == cFileName) {
        goto exit;
    }

    // 打开 WAVE 文件
    wav = wav_open(cFileName, WAV_READ, &error);

    // 释放文件名
    env->ReleaseStringUTFChars(fileName, cFileName);

    // 错误检查
    if (0 == wav) {
        ThrowException(env, JAVA_LANG_IOEXCEPTION, wav_strerror(error));
    }

    exit:
    return wav;
}

static void CloseWaveFile(WAV wav) {
    if (0 != wav) {
        wav_close(wav);
    }
}

static const char *ResultToString(SLresult result) {
    const char *str = 0;
    switch (result) {
        case SL_RESULT_SUCCESS:
            str = "Success";
            break;
        case SL_RESULT_PRECONDITIONS_VIOLATED:
            str = "Preconditions violated";
            break;
        case SL_RESULT_PARAMETER_INVALID:
            str = "Parameter invalid";
            break;
        case SL_RESULT_MEMORY_FAILURE:
            str = "Memory failure";
            break;
        case SL_RESULT_RESOURCE_ERROR:
            str = "Resource error";
            break;
        case SL_RESULT_RESOURCE_LOST:
            str = "Resource lost";
            break;
        case SL_RESULT_IO_ERROR:
            str = "IO error";
            break;
        case SL_RESULT_BUFFER_INSUFFICIENT:
            str = "Buffer insufficient";
            break;
        case SL_RESULT_CONTENT_CORRUPTED:
            str = "Success";
            break;
        case SL_RESULT_CONTENT_NOT_FOUND:
            str = "Content not found";
            break;
        case SL_RESULT_PERMISSION_DENIED:
            str = "Permission denied";
            break;
        case SL_RESULT_FEATURE_UNSUPPORTED:
            str = "Feature unsupported";
            break;
        case SL_RESULT_INTERNAL_ERROR:
            str = "Internal error";
            break;
        case SL_RESULT_UNKNOWN_ERROR:
            str = "Unknown error";
            break;
        case SL_RESULT_OPERATION_ABORTED:
            str = "Operation aborted";
            break;
        case SL_RESULT_CONTROL_LOST:
            str = "Control lost";
            break;
        default:
            str = "Unknown code";
    }
    return str;
}

/**
 * 检查结果是否出错，并抛出含错误信息的IOException
 * @param env
 * @param result
 * @return
 */
static bool CheckError(JNIEnv *env, SLresult result) {
    bool isError = false;

    // 如果发生错误
    if (SL_RESULT_SUCCESS != result) {
        ThrowException(env, JAVA_LANG_IOEXCEPTION, ResultToString(result));
        isError = true;
    }
    return isError;
}

/**
 * 创建 OpenSL ES 引擎
 * @param env
 * @param engineObject
 */
static void CreateEngine(JNIEnv *env, SLObjectItf &engineObject) {

    /*
     * Android 中 OpenSL ES 被设计为是线程安全的，
     * 所以该选项请求将被忽略，但它应该保证源代码可被移植到其他平台
     */
    SLEngineOption engineOptions[] = {(SLuint32) SL_ENGINEOPTION_THREADSAFE,
                                      (SLuint32) SL_BOOLEAN_TRUE};

    // 创建 OpenSL ES 引擎对象
    SLresult result = slCreateEngine(&engineObject, ARRAY_LEN(engineOptions), engineOptions,
                                     0, // 没有接口
                                     0, // 没有接口
                                     0); // 无须提供
    // 错误检查
    CheckError(env, result);
}

/**
 * 实现给定的对象， Object 在使用前应该被实现
 * @param env
 * @param object
 */
static void RealizeObject(JNIEnv *env, SLObjectItf object) {
    SLresult result = (*object)->Realize(object,
                                         SL_BOOLEAN_FALSE); // No async, blocking call

    // 错误检查
    CheckError(env, result);
}

/**
 * 销毁给定的对象
 * @param object
 */
static void DestroyObject(SLObjectItf &object) {
    if (0 != object) {
        (*(SLObjectItf) object)->Destroy(object);
    }
    object = 0;
}

/**
 * 从给定引擎对象中获得引擎接口以便从该引擎中创建其他对象
 * @param env
 * @param engineObject
 * @param engineEngine
 */
static void GetEngineInterface(JNIEnv *env, SLObjectItf &engineObject, SLEngineItf &engineEngine) {
    // 获得引擎接口
    SLresult result = (*(SLObjectItf) engineObject)->GetInterface(engineObject, SL_IID_ENGINE,
                                                                  &engineEngine);

    // 检查错误
    CheckError(env, result);
}

/**
 * 创建和输出混合对象
 * @param env
 * @param engineEngine
 * @param outputMixObject
 */
static void CreateOutputMix(JNIEnv *env, SLEngineItf engineEngine, SLObjectItf &outputMixObject) {
    // 创建输出混合对象
    SLresult result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject,
                                                       0, // 没有接口
                                                       0, // 没有接口
                                                       0); // 无须提供

    // 检查错误
    CheckError(env, result);
}

/**
 * 释放播放器缓冲区
 * @param buffers
 */
static void FreePlayerBuffer(unsigned char *&buffers) {
    if (0 != buffers) {
        delete buffers;
        buffers = 0;
    }
}

/**
 * 初始化播放器缓冲区
 * @param env
 * @param wav
 * @param buffer
 * @param bufferSize
 */
static void InitPlayerBuffer(JNIEnv *env, WAV wav, unsigned char *&buffer, size_t &bufferSize) {
    // 计算缓冲区的大小
    bufferSize = wav_get_channels(wav) * wav_get_rate(wav) * wav_get_bits(wav);

    // 初始化 buffer
    buffer = new unsigned char[bufferSize];

    if (0 == buffer) {
        ThrowException(env, JAVA_LANG_OUTOFMEMORYERROR, "buffer");
    }
}

/**
 * 创建缓冲区队列音频播放器
 * @param wav
 * @param engineEngine
 * @param outputMixObject
 * @param audioPlayerObject
 */
static void CreateBufferQueueAudioPlayer(WAV wav,
                                         SLEngineItf engineEngine,
                                         SLObjectItf outputMixObject,
                                         SLObjectItf &audioPlayerObject) {
    // Android针对数据源的简单缓冲区队列定位器
    SLDataLocator_AndroidSimpleBufferQueue dataSourceLocator = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, // 定位器类型
            1}; // 缓冲区数

    // PCM 数据源格式
    SLDataFormat_PCM dataSourceFormat = {
            SL_DATAFORMAT_PCM, // 格式类型
            wav_get_channels(wav), // 通道数
            wav_get_rate(wav) * 1000, // 毫赫兹 / 秒 的样本数
            wav_get_bits(wav), // 每个样本的位置
            wav_get_bits(wav), // 容器大小
            SL_SPEAKER_FRONT_CENTER, // 通道屏蔽
            SL_BYTEORDER_LITTLEENDIAN // 字节顺序额
    };

    // 数据源是含有 PCM 格式的简单缓冲区队列
    SLDataSource dataSource = {
            &dataSourceLocator, // 数据定位器
            &dataSourceFormat // 数据格式
    };

    // 针对数据接收器的输出混合定位器
    SLDataLocator_OutputMix dataSinkLocator = {
            SL_DATALOCATOR_OUTPUTMIX, // 定位器类型
            outputMixObject // 输出混合
    };

    // 数据定位器是一个输出混合
    SLDataSink dataSink = {
            &dataSinkLocator, // 定位器
            0 // 格式
    };

    // 需要的接口
    SLInterfaceID interfaceIds[] = {
            SL_IID_BUFFERQUEUE
    };

    // 需要的接口。如果所需要的接口不要用，请求将失败
    SLboolean requiredInterfaces[] = {
            SL_BOOLEAN_TRUE // for SL_IID_BUFFERQUEUE
    };

    // 创建音频播放器对象
    SLresult result = (*engineEngine)->CreateAudioPlayer(
            engineEngine,
            &audioPlayerObject,
            &dataSource,
            &dataSink,
            ARRAY_LEN(interfaceIds),
            interfaceIds,
            requiredInterfaces
    );
}

/**
 * 获得音频播放器缓冲区队列接口
 * @param env
 * @param audioPlayerObject
 * @param audioPlayerBufferQueue
 */
static void GetAudioPlayerBufferQueueInterface(
        JNIEnv *env,
        SLObjectItf audioPlayerObject,
        SLAndroidSimpleBufferQueueItf &audioPlayerBufferQueue) {
    // 获得缓冲区队列接口
    SLresult result = (*audioPlayerObject)->GetInterface(audioPlayerObject, SL_IID_BUFFERQUEUE,
                                                         &audioPlayerBufferQueue);

    // 检查错误
    CheckError(env, result);
}

/**
 * 销毁播放器上下文
 * @param ctx
 */
static void DestroyContext(PlayerContext *&ctx) {
    // 销毁音频播放器对象
    DestroyObject(ctx->audioPlayerObject);

    // 释放播放器缓冲区
    FreePlayerBuffer(ctx->buffer);

    // 销毁输出混合对象
    DestroyObject(ctx->outputMixObject);

    // 销毁引擎实例
    DestroyObject(ctx->engineObject);

    // 关闭 WAVE 文件
    CloseWaveFile(ctx->wav);

    // 释放上下文
    delete ctx;
    ctx = 0;
}

/**
 * 当一个缓冲区完成播放是获得调用
 * @param audioPlayerBufferQueue
 * @param context
 */
static void PlayerCallback(SLAndroidSimpleBufferQueueItf audioPlayerBufferQueue, void *context) {
    // 获得播放器上下文
    PlayerContext *ctx = static_cast<PlayerContext *>(context);

    // 读取数据
    ssize_t readSize = wav_read_data(
            ctx->wav,
            ctx->buffer,
            ctx->bufferSize
    );

    // 如果数据被读取
    if (0 < readSize) {
        (*audioPlayerBufferQueue)->Enqueue(
                audioPlayerBufferQueue,
                ctx->buffer,
                readSize
        );
    } else {
        DestroyContext(ctx);
    }
}

/**
 * 注册播放器回调
 * @param env
 * @param audioPlayerBufferQueue
 * @param ctx
 */
static void RegisterPlayerCallback(
        JNIEnv *env,
        SLAndroidSimpleBufferQueueItf audioPlayerBufferQueue,
        PlayerContext *ctx) {
    // 注册播放器回调
    SLresult result = (*audioPlayerBufferQueue)->RegisterCallback(
            audioPlayerBufferQueue,
            PlayerCallback,
            ctx); // player context

    // 错误检查
    CheckError(env, result);
}

/**
 * 获得音频播放器播放接口
 * @param env
 * @param audioPlayerObject
 * @param audioPlayerPlay
 */
static void GetAudioPlayerPlayInterface(JNIEnv *env, SLObjectItf audioPlayerObject,
                                        SLPlayItf &audioPlayerPlay) {
    SLresult result = (*audioPlayerObject)->GetInterface(
            audioPlayerObject,
            SL_IID_PLAY,
            &audioPlayerPlay
    );

    // 错误检查
    CheckError(env, result);
}

/**
 * 把音频播放器设置为播放状态
 * @param env
 * @param audioPlayerPlay
 */
static void SetAudioPlayerStatePlaying(JNIEnv *env, SLPlayItf audioPlayerPlay) {
    // 把音频播放器状态设置为播放
    SLresult result = (*audioPlayerPlay)->SetPlayState(audioPlayerPlay, SL_PLAYSTATE_PLAYING);

    // 检查错误
    CheckError(env, result);
}

void Java_com_liuuuu_wavplayer_MainActivity_play
        (JNIEnv *env, jobject obj, jstring fileName) {
    PlayerContext *ctx = new PlayerContext();

    // 打开 WAVE 文件
    ctx->wav = OpenWaveFile(env, fileName);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 创建 OpenSL ES 引擎
    CreateEngine(env, ctx->engineObject);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 实现引擎对象
    RealizeObject(env, ctx->engineObject);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 获得引擎接口
    GetEngineInterface(env, ctx->engineObject, ctx->engineEngine);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 创建输出混合对象
    CreateOutputMix(env, ctx->engineEngine, ctx->outputMixObject);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 实现输出混合对象
    RealizeObject(env, ctx->outputMixObject);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 初始化缓冲区
    InitPlayerBuffer(env, ctx->wav, ctx->buffer, ctx->bufferSize);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 创建缓冲区队列音频播放器对象
    CreateBufferQueueAudioPlayer(ctx->wav, ctx->engineEngine, ctx->outputMixObject,
                                 ctx->audioPlayerObject);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 实现音频播放器对象
    RealizeObject(env, ctx->audioPlayerObject);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 获得音频播放器缓冲区队列接口
    GetAudioPlayerBufferQueueInterface(env, ctx->audioPlayerObject, ctx->audioPlayerBufferQueue);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 注册播放器回调函数
    RegisterPlayerCallback(env, ctx->audioPlayerBufferQueue, ctx);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 获得音频播放器播放接口
    GetAudioPlayerPlayInterface(env, ctx->audioPlayerObject, ctx->audioPlayerPlay);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 设置音频播放器为播放状态
    SetAudioPlayerStatePlaying(env, ctx->audioPlayerPlay);
    if (0 != env->ExceptionOccurred()) goto exit;

    // 将第一个缓冲区入队来启动运行
    PlayerCallback(ctx->audioPlayerBufferQueue, ctx);

    exit:
    if (0 != env->ExceptionOccurred()) {
        DestroyContext(ctx);
    }
}