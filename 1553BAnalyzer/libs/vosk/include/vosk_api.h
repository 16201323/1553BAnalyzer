/**
 * @file vosk_api.h
 * @brief Vosk语音识别引擎C API头文件
 *
 * 本文件定义了Vosk离线语音识别引擎的C语言接口。
 * Vosk是一个开源的语音识别工具包，支持离线运行，
 * 提供实时流式识别能力。
 *
 * 主要接口：
 * - VoskModel: 语音识别模型，包含声学模型和语言模型
 * - VoskRecognizer: 语音识别器，处理音频数据并返回识别结果
 * - VoskSpkModel: 说话人识别模型
 *
 * 使用流程：
 * 1. vosk_model_new() 加载模型
 * 2. vosk_recognizer_new() 创建识别器
 * 3. vosk_recognizer_accept_waveform() 送入音频数据
 * 4. vosk_recognizer_partial_result() 获取实时部分结果
 * 5. vosk_recognizer_final_result() 获取最终识别结果
 * 6. vosk_recognizer_free() / vosk_model_free() 释放资源
 *
 * @note 本文件从Vosk官方仓库获取，版本0.3.45
 * @see https://github.com/alphacep/vosk-api
 */

#ifndef VOSK_API_H
#define VOSK_API_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 语音识别模型结构体
 *
 * 存储语音识别所需的所有静态数据，包括声学模型、
 * 语言模型和发音词典等。该结构体是线程安全的，
 * 可以被多个识别器线程共享使用。
 */
typedef struct VoskModel VoskModel;

/**
 * @brief 说话人识别模型结构体
 *
 * 包含说话人识别所需的数据，用于声纹识别功能。
 */
typedef struct VoskSpkModel VoskSpkModel;

/**
 * @brief 语音识别器结构体
 *
 * 处理音频数据并返回识别结果。每个识别器实例
 * 绑定一个模型，处理单路音频流。
 */
typedef struct VoskRecognizer VoskRecognizer;


/* ========== 模型管理接口 ========== */

/**
 * @brief 加载语音识别模型
 * @param model_path 模型目录路径（必须包含am、conf、graph等子目录）
 * @return 模型指针，失败返回NULL
 */
VoskModel *vosk_model_new(const char *model_path);

/**
 * @brief 释放语音识别模型
 * @param model 要释放的模型指针
 */
void vosk_model_free(VoskModel *model);

/**
 * @brief 检查模型是否能识别指定单词
 * @param model 模型指针
 * @param word 要检查的单词
 * @return 1表示能识别，0表示不能
 */
int vosk_model_find_word(VoskModel *model, const char *word);


/* ========== 说话人模型接口 ========== */

/**
 * @brief 加载说话人识别模型
 * @param model_path 模型路径
 * @return 说话人模型指针，失败返回NULL
 */
VoskSpkModel *vosk_spk_model_new(const char *model_path);

/**
 * @brief 释放说话人识别模型
 * @param model 要释放的说话人模型指针
 */
void vosk_spk_model_free(VoskSpkModel *model);


/* ========== 识别器管理接口 ========== */

/**
 * @brief 创建语音识别器
 * @param model 语音识别模型指针
 * @param sample_rate 音频采样率（通常为16000）
 * @return 识别器指针，失败返回NULL
 */
VoskRecognizer *vosk_recognizer_new(VoskModel *model, float sample_rate);

/**
 * @brief 创建带说话人识别的语音识别器
 * @param model 语音识别模型指针
 * @param spk_model 说话人识别模型指针
 * @param sample_rate 音频采样率
 * @return 识别器指针
 */
VoskRecognizer *vosk_recognizer_new_spk(VoskModel *model, VoskSpkModel *spk_model, float sample_rate);

/**
 * @brief 创建带语法约束的语音识别器
 * @param model 语音识别模型指针
 * @param sample_rate 音频采样率
 * @param grammar JSON格式的语法列表，如"[\"yes\", \"no\"]"
 * @return 识别器指针
 */
VoskRecognizer *vosk_recognizer_new_grm(VoskModel *model, float sample_rate, const char *grammar);

/**
 * @brief 设置识别器的单词列表（用于词级别的时间戳）
 * @param recognizer 识别器指针
 * @param words 单词列表JSON字符串
 */
void vosk_recognizer_set_words(VoskRecognizer *recognizer, int words);

/**
 * @brief 设置识别器的部分结果单词级别时间戳
 * @param recognizer 识别器指针
 * @param partial_words 是否启用部分结果单词时间戳
 */
void vosk_recognizer_set_partial_words(VoskRecognizer *recognizer, int partial_words);

/**
 * @brief 设置N最佳列表大小
 * @param recognizer 识别器指针
 * @param nbest N最佳列表大小
 */
void vosk_recognizer_set_nlsml(VoskRecognizer *recognizer, int nlsml);

/**
 * @brief 释放语音识别器
 * @param recognizer 要释放的识别器指针
 */
void vosk_recognizer_free(VoskRecognizer *recognizer);


/* ========== 音频处理接口 ========== */

/**
 * @brief 送入音频数据进行识别
 * @param recognizer 识别器指针
 * @param data 音频数据（16位有符号整数PCM格式，单声道）
 * @param length 数据长度（字节数）
 * @return 0表示需要更多数据，1表示检测到语音端点（可获取最终结果）
 */
int vosk_recognizer_accept_waveform(VoskRecognizer *recognizer, const char *data, int length);

/**
 * @brief 送入短整型音频数据
 * @param recognizer 识别器指针
 * @param data 音频数据（16位有符号整数）
 * @param length 采样点数（不是字节数）
 * @return 0表示需要更多数据，1表示检测到语音端点
 */
int vosk_recognizer_accept_waveform_s(VoskRecognizer *recognizer, const short *data, int length);

/**
 * @brief 送入浮点型音频数据
 * @param recognizer 识别器指针
 * @param data 音频数据（32位浮点数，范围-1.0到1.0）
 * @param length 采样点数
 * @return 0表示需要更多数据，1表示检测到语音端点
 */
int vosk_recognizer_accept_waveform_f(VoskRecognizer *recognizer, const float *data, int length);


/* ========== 结果获取接口 ========== */

/**
 * @brief 获取当前的部分识别结果（实时结果）
 *
 * 返回JSON格式字符串，包含"text"字段表示当前正在识别的文字。
 * 调用者需要调用vosk_recognizer_free_result()释放返回的字符串。
 *
 * @param recognizer 识别器指针
 * @return JSON格式的部分识别结果字符串
 */
const char *vosk_recognizer_partial_result(VoskRecognizer *recognizer);

/**
 * @brief 获取最终的识别结果
 *
 * 当检测到语音端点时调用，返回完整的识别结果。
 * 返回JSON格式字符串，包含"text"字段。
 * 调用者需要调用vosk_recognizer_free_result()释放返回的字符串。
 *
 * @param recognizer 识别器指针
 * @return JSON格式的最终识别结果字符串
 */
const char *vosk_recognizer_final_result(VoskRecognizer *recognizer);

/**
 * @brief 获取带单词时间戳的最终识别结果
 * @param recognizer 识别器指针
 * @return JSON格式的识别结果，包含单词级时间戳
 */
const char *vosk_recognizer_final_result_s(VoskRecognizer *recognizer);

/**
 * @brief 释放识别结果字符串的内存
 * @param result 要释放的结果字符串指针
 */
void vosk_recognizer_free_result(const char *result);

/**
 * @brief 重置识别器状态
 *
 * 在识别过程中可以调用此函数重置识别器，
 * 通常在切换说话人或开始新的识别会话时使用。
 *
 * @param recognizer 识别器指针
 */
void vosk_recognizer_reset(VoskRecognizer *recognizer);


/* ========== GPU加速接口 ========== */

/**
 * @brief 设置GPU设备ID（用于GPU加速推理）
 * @param model 模型指针
 * @param gpu_device_id GPU设备ID（-1表示使用CPU）
 */
void vosk_gpu_init(void);

/**
 * @brief 初始化GPU加速
 */
void vosk_gpu_thread_init(void);

/**
 * @brief 释放GPU资源
 */
void vosk_gpu_thread_exit(void);


#ifdef __cplusplus
}
#endif

#endif /* VOSK_API_H */
