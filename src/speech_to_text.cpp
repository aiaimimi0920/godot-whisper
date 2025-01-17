#include "speech_to_text.h"
#include <libsamplerate/src/samplerate.h>
#include <atomic>
#include <cmath>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <string>
#include <vector>

uint32_t _resample_audio_buffer(
		const float *p_src, const uint32_t p_src_frame_count,
		const uint32_t p_src_samplerate, const uint32_t p_target_samplerate,
		float *p_dst) {
	if (p_src_samplerate != p_target_samplerate) {
		SRC_DATA src_data;

		src_data.data_in = p_src;
		src_data.data_out = p_dst;

		src_data.input_frames = p_src_frame_count;
		src_data.src_ratio = (double)p_target_samplerate / (double)p_src_samplerate;
		src_data.output_frames = int(p_src_frame_count * src_data.src_ratio);

		src_data.end_of_input = 0;
		int error = src_simple(&src_data, SRC_SINC_BEST_QUALITY, 1);
		if (error != 0) {
			ERR_PRINT(String(src_strerror(error)));
			return 0;
		}
		return src_data.output_frames_gen;
	} else {
		memcpy(p_dst, p_src,
				static_cast<size_t>(p_src_frame_count) * sizeof(float));
		return p_src_frame_count;
	}
}

void _vector2_array_to_float_array(const uint32_t &p_mix_frame_count,
		const Vector2 *p_process_buffer_in,
		float *p_process_buffer_out) {
	for (size_t i = 0; i < p_mix_frame_count; i++) {
		p_process_buffer_out[i] = (p_process_buffer_in[i].x + p_process_buffer_in[i].y) / 2.0;
	}
}

void high_pass_filter(std::vector<float> &data, float cutoff, float sample_rate) {
	const float rc = 1.0f / (2.0f * Math_PI * cutoff);
	const float dt = 1.0f / sample_rate;
	const float alpha = dt / (rc + dt);

	float y = data[0];

	for (size_t i = 1; i < data.size(); i++) {
		y = alpha * (y + data[i] - data[i - 1]);
		data[i] = y;
	}
}

/** Check if speech is ending. */
bool vad_simple(std::vector<float> &pcmf32, int sample_rate, int last_ms, float vad_thold, float freq_thold, bool verbose) {
	const int n_samples = pcmf32.size();
	const int n_samples_last = (sample_rate * last_ms) / 1000;

	if (n_samples_last >= n_samples) {
		// not enough samples - assume no speech
		return false;
	}

	if (freq_thold > 0.0f) {
		high_pass_filter(pcmf32, freq_thold, sample_rate);
	}

	float energy_all = 0.0f;
	float energy_last = 0.0f;

	for (int i = 0; i < n_samples; i++) {
		energy_all += fabsf(pcmf32[i]);

		if (i >= n_samples - n_samples_last) {
			energy_last += fabsf(pcmf32[i]);
		}
	}

	energy_all /= n_samples;
	if (n_samples_last != 0) {
		energy_last /= n_samples_last;
	}

	if (verbose) {
		fprintf(stderr, "%s: energy_all: %f, energy_last: %f, vad_thold: %f, freq_thold: %f\n", __func__, energy_all, energy_last, vad_thold, freq_thold);
	}

	if ((energy_all < 0.0001f && energy_last < 0.0001f) == false || energy_last > vad_thold * energy_all) {
		return false;
	}
	return true;
}

SpeechToText *SpeechToText::singleton = nullptr;

SpeechToText *SpeechToText::get_singleton() {
	return singleton;
}

SpeechToText::SpeechToText() {
	singleton = this;
}

void SpeechToText::start_listen() {
	if (this->worker == nullptr) {
		this->worker = memnew(Thread);
	}

	if (this->is_running == false) {
		this->is_running = true;
		this->worker->start(callable_mp(this, &SpeechToText::run), Thread::Priority::PRIORITY_NORMAL);
		t_last_iter = Time::get_singleton()->get_ticks_msec();
	}
}

void SpeechToText::stop_listen() {
	this->is_running = false;
	if (this->worker != nullptr) {
		this->worker = nullptr;
	}
}

void SpeechToText::set_language(int p_language) {
	language = (Language)p_language;
	params.language = language_to_code(language);
	if (is_running) {
		this->full_params.language = params.language.c_str();
	}
}

int SpeechToText::get_language() {
	return language;
}

std::string SpeechToText::language_to_code(Language language) {
	switch (language) {
		case Auto:
			return "auto";
		case English:
			return "en";
		case Chinese:
			return "zh";
		case German:
			return "de";
		case Spanish:
			return "es";
		case Russian:
			return "ru";
		case Korean:
			return "ko";
		case French:
			return "fr";
		case Japanese:
			return "ja";
		case Portuguese:
			return "pt";
		case Turkish:
			return "tr";
		case Polish:
			return "pl";
		case Catalan:
			return "ca";
		case Dutch:
			return "nl";
		case Arabic:
			return "ar";
		case Swedish:
			return "sv";
		case Italian:
			return "it";
		case Indonesian:
			return "id";
		case Hindi:
			return "hi";
		case Finnish:
			return "fi";
		case Vietnamese:
			return "vi";
		case Hebrew:
			return "he";
		case Ukrainian:
			return "uk";
		case Greek:
			return "el";
		case Malay:
			return "ms";
		case Czech:
			return "cs";
		case Romanian:
			return "ro";
		case Danish:
			return "da";
		case Hungarian:
			return "hu";
		case Tamil:
			return "ta";
		case Norwegian:
			return "no";
		case Thai:
			return "th";
		case Urdu:
			return "ur";
		case Croatian:
			return "hr";
		case Bulgarian:
			return "bg";
		case Lithuanian:
			return "lt";
		case Latin:
			return "la";
		case Maori:
			return "mi";
		case Malayalam:
			return "ml";
		case Welsh:
			return "cy";
		case Slovak:
			return "sk";
		case Telugu:
			return "te";
		case Persian:
			return "fa";
		case Latvian:
			return "lv";
		case Bengali:
			return "bn";
		case Serbian:
			return "sr";
		case Azerbaijani:
			return "az";
		case Slovenian:
			return "sl";
		case Kannada:
			return "kn";
		case Estonian:
			return "et";
		case Macedonian:
			return "mk";
		case Breton:
			return "br";
		case Basque:
			return "eu";
		case Icelandic:
			return "is";
		case Armenian:
			return "hy";
		case Nepali:
			return "ne";
		case Mongolian:
			return "mn";
		case Bosnian:
			return "bs";
		case Kazakh:
			return "kk";
		case Albanian:
			return "sq";
		case Swahili:
			return "sw";
		case Galician:
			return "gl";
		case Marathi:
			return "mr";
		case Punjabi:
			return "pa";
		case Sinhala:
			return "si";
		case Khmer:
			return "km";
		case Shona:
			return "sn";
		case Yoruba:
			return "yo";
		case Somali:
			return "so";
		case Afrikaans:
			return "af";
		case Occitan:
			return "oc";
		case Georgian:
			return "ka";
		case Belarusian:
			return "be";
		case Tajik:
			return "tg";
		case Sindhi:
			return "sd";
		case Gujarati:
			return "gu";
		case Amharic:
			return "am";
		case Yiddish:
			return "yi";
		case Lao:
			return "lo";
		case Uzbek:
			return "uz";
		case Faroese:
			return "fo";
		case Haitian_Creole:
			return "ht";
		case Pashto:
			return "ps";
		case Turkmen:
			return "tk";
		case Nynorsk:
			return "nn";
		case Maltese:
			return "mt";
		case Sanskrit:
			return "sa";
		case Luxembourgish:
			return "lb";
		case Myanmar:
			return "my";
		case Tibetan:
			return "bo";
		case Tagalog:
			return "tl";
		case Malagasy:
			return "mg";
		case Assamese:
			return "as";
		case Tatar:
			return "tt";
		case Hawaiian:
			return "haw";
		case Lingala:
			return "ln";
		case Hausa:
			return "ha";
		case Bashkir:
			return "ba";
		case Javanese:
			return "jw";
		case Sundanese:
			return "su";
		case Cantonese:
			return "yue";
	}
}

void SpeechToText::set_language_model(Ref<WhisperResource> p_model) {
	model = p_model;
	load_model();
}

void SpeechToText::load_model() {
	whisper_free(context_instance);
	if (model.is_null()) {
		return;
	}
	PackedByteArray data = model->get_content();
	if (data.is_empty()) {
		return;
	}
	context_instance = whisper_init_from_buffer_with_params((void *)(data.ptr()), data.size(), context_parameters);
	UtilityFunctions::print(whisper_print_system_info());
}

void SpeechToText::set_use_gpu(bool use_gpu) {
	context_parameters.use_gpu = use_gpu;
	load_model();
}

SpeechToText::~SpeechToText() {
	singleton = nullptr;
	stop_listen();
	whisper_free(context_instance);
}
/** Add audio data in PCM f32 format. */
void SpeechToText::add_audio_buffer(PackedVector2Array buffer) {
	s_mutex.lock();
	int buffer_len = buffer.size();
	float *buffer_float = (float *)memalloc(sizeof(float) * buffer_len);
	float *resampled_float = (float *)memalloc(sizeof(float) * buffer_len * SPEECH_SETTING_SAMPLE_RATE / AudioServer::get_singleton()->get_mix_rate());
	_vector2_array_to_float_array(buffer_len, buffer.ptr(), buffer_float);
	// Speaker frame.
	int result_size = _resample_audio_buffer(
			buffer_float, // Pointer to source buffer
			buffer_len, // Size of source buffer * sizeof(float)
			AudioServer::get_singleton()->get_mix_rate(), // Source sample rate
			SPEECH_SETTING_SAMPLE_RATE, // Target sample rate
			resampled_float);

	std::vector<float> data(resampled_float, resampled_float + result_size);

	const int vad_last_ms = 0;
	const float vad_thold = params.vad_thold;
	const float freq_thold = params.freq_thold;
	bool is_empty_array = vad_simple(data, WHISPER_SAMPLE_RATE, vad_last_ms, vad_thold, freq_thold, false);

	if (is_empty_array == false) {
		s_queued_pcmf32.insert(s_queued_pcmf32.end(), data.begin(), data.end());
	}
	memfree(buffer_float);
	memfree(resampled_float);
	s_mutex.unlock();
}

/** Run Whisper in its own thread to not block the main thread. */
void SpeechToText::run() {
	SpeechToText *speech_to_text_obj = SpeechToText::get_singleton();
	whisper_full_params whisper_params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
	// See here for example https://github.com/ggerganov/whisper.cpp/blob/master/examples/stream/stream.cpp#L302
	whisper_params.max_len = 0;
	whisper_params.print_progress = false;
	whisper_params.print_special = false;
	whisper_params.print_realtime = false;
	// This is set later on based on how much frames we can process
	whisper_params.duration_ms = 0;
	whisper_params.print_timestamps = false;
	whisper_params.translate = speech_to_text_obj->params.translate;
	whisper_params.single_segment = false;
	whisper_params.no_timestamps = false;
	whisper_params.token_timestamps = true;
	whisper_params.max_tokens = speech_to_text_obj->params.max_tokens;
	whisper_params.language = speech_to_text_obj->params.language.c_str();
	whisper_params.n_threads = speech_to_text_obj->params.n_threads;
	whisper_params.speed_up = speech_to_text_obj->params.speed_up;
	whisper_params.prompt_tokens = nullptr;
	whisper_params.prompt_n_tokens = 0;
	whisper_params.suppress_non_speech_tokens = true;
	whisper_params.suppress_blank = true;
	whisper_params.entropy_thold = speech_to_text_obj->params.entropy_threshold;
	whisper_params.temperature = 0.0;
	whisper_params.no_context = true;

	/**
	 * Experimental optimization: Reduce audio_ctx to 15s (half of the chunk
	 * size whisper is designed for) to speed up 2x.
	 * https://github.com/ggerganov/whisper.cpp/issues/137#issuecomment-1318412267
	 */
	whisper_params.audio_ctx = 768;

	speech_to_text_obj->full_params = whisper_params;

	/* When more than this amount of audio received, run an iteration. */
	const int trigger_ms = 400;
	const int n_samples_trigger = (trigger_ms / 1000.0) * WHISPER_SAMPLE_RATE;
	/**
	 * When more than this amount of audio accumulates in the audio buffer,
	 * force finalize current audio context and clear the buffer. Note that
	 * VAD may finalize an iteration earlier.
	 */
	// This is recommended to be smaller than the time wparams.audio_ctx
	// represents so an iteration can fit in one chunk.
	const int iter_threshold_ms = trigger_ms * 35;
	const int n_samples_iter_threshold = (iter_threshold_ms / 1000.0) * WHISPER_SAMPLE_RATE;

	/**
	 * ### Reminders
	 *
	 * - Note that whisper designed to process audio in 30-second chunks, and
	 *   the execution time of processing smaller chunks may not be shorter.
	 * - The design of trigger and threshold allows inputing audio data at
	 *   arbitrary rates with zero config. Inspired by Assembly.ai's
	 *   real-time transcription API
	 *   (https://github.com/misraturp/Real-time-transcription-from-microphone/blob/main/speech_recognition.py)
	 */

	/* VAD parameters */
	// The most recent 3s.
	const int vad_window_s = 3;
	const int n_samples_vad_window = WHISPER_SAMPLE_RATE * vad_window_s;
	// In VAD, compare the energy of the last 500ms to that of the total 3s.
	const int vad_last_ms = 500;
	// Keep the last 0.5s of an iteration to the next one for better
	// transcription at begin/end.
	const int n_samples_keep_iter = WHISPER_SAMPLE_RATE * 0.5;
	const float vad_thold = speech_to_text_obj->params.vad_thold;
	const float freq_thold = speech_to_text_obj->params.freq_thold;

	/* Audio buffer */
	std::vector<float> pcmf32;

	int empty_iter_count = 0;
	bool need_close_segment = false;
	/* Processing loop */
	while (speech_to_text_obj->is_running) {
		{
			speech_to_text_obj->s_mutex.lock();
			need_close_segment = false;
			if (speech_to_text_obj->s_queued_pcmf32.size() < WHISPER_SAMPLE_RATE) {
				empty_iter_count += 1;
				if (empty_iter_count >= 20 && pcmf32.size() > 0) {
					need_close_segment = true;
					empty_iter_count = 0;
				} else {
					empty_iter_count = empty_iter_count % 20;
					speech_to_text_obj->s_mutex.unlock();
					OS::get_singleton()->delay_msec(50);
					continue;
				}
			}
			speech_to_text_obj->s_mutex.unlock();
		}
		{
			speech_to_text_obj->s_mutex.lock();
			if (speech_to_text_obj->s_queued_pcmf32.size() > 2 * n_samples_iter_threshold) {
				WARN_PRINT("Too much audio is going to be processed, result may not come out in real time");
			}
			speech_to_text_obj->s_mutex.unlock();
		}
		{
			speech_to_text_obj->s_mutex.lock();
			pcmf32.insert(pcmf32.end(), speech_to_text_obj->s_queued_pcmf32.begin(), speech_to_text_obj->s_queued_pcmf32.end());
			speech_to_text_obj->s_queued_pcmf32.clear();
			speech_to_text_obj->s_mutex.unlock();
		}

		if (!speech_to_text_obj->context_instance) {
			ERR_PRINT("Context instance is null");
			continue;
		}
		float time_started = Time::get_singleton()->get_ticks_msec();
		{
			whisper_params.duration_ms = pcmf32.size() * 1000.0f / WHISPER_SAMPLE_RATE;
			int ret = whisper_full(speech_to_text_obj->context_instance, speech_to_text_obj->full_params, pcmf32.data(), pcmf32.size());
			if (ret != 0) {
				ERR_PRINT("Failed to process audio, returned " + rtos(ret));
				continue;
			}
		}
		{
			transcribed_msg msg;
			/**
			 * Simple VAD from the "stream" example in whisper.cpp
			 * https://github.com/ggerganov/whisper.cpp/blob/231bebca7deaf32d268a8b207d15aa859e52dbbe/examples/stream/stream.cpp#L378
			 */
			bool speech_has_end = false;
			/* Need enough accumulated audio to do VAD. */
			if ((int)pcmf32.size() >= n_samples_vad_window) {
				std::vector<float> pcmf32_window(pcmf32.end() - n_samples_vad_window, pcmf32.end());
				speech_has_end = vad_simple(pcmf32_window, WHISPER_SAMPLE_RATE, vad_last_ms,
						vad_thold, freq_thold, false);
				if (speech_has_end) {
					printf("speech end detected\n");
				}
			}
			if (need_close_segment) {
				if (vad_simple(pcmf32, WHISPER_SAMPLE_RATE, 0, vad_thold, freq_thold, false)) {
					msg.text = "";
				}
				speech_has_end = true;
			}
			const int n_segments = whisper_full_n_segments(speech_to_text_obj->context_instance);
			int64_t delete_target_t = 0;
			bool find_delete_target_t = false;
			int64_t target_index = 0;

			int64_t half_t = 0;
			if (n_segments > 0) {
				const int cur_n_tokens = whisper_full_n_tokens(speech_to_text_obj->context_instance, n_segments - 1);
				auto cur_last_token = whisper_full_get_token_data(speech_to_text_obj->context_instance, n_segments - 1, cur_n_tokens - 1);
				half_t = cur_last_token.t1 * 1.0 / 2.0;
			}
			for (int i = 0; i < n_segments; ++i) {
				const int n_tokens = whisper_full_n_tokens(speech_to_text_obj->context_instance, i);
				for (int j = 0; j < n_tokens; j++) {
					auto token = whisper_full_get_token_data(speech_to_text_obj->context_instance, i, j);
					auto text = whisper_full_get_token_text(speech_to_text_obj->context_instance, i, j);
					// Idea from https://github.com/yum-food/TaSTT/blob/dbb2f72792e2af3ff220313f84bf76a9a1ddbeb4/Scripts/transcribe_v2.py#L457C17-L462C25
					if (find_delete_target_t == false) {
						String cur_text = String(text);
						if (cur_text.begins_with("[_TT_") || cur_text == "," || cur_text == "." || cur_text == "?" || cur_text == "!" || cur_text == "，" || cur_text == "。" || cur_text == "？" || cur_text == "！") {
							if (token.t1 < half_t) {
								delete_target_t = token.t1;
								target_index = msg.text.size() + cur_text.length();
								msg.text += text;
							} else {
								if (delete_target_t == 0) {
									delete_target_t = token.t1;
									msg.text += text;
									if (speech_has_end == false) {
										msg.text += "{SPLIT}";
									}

								} else {
									if (speech_has_end == false) {
										msg.text.insert(target_index, "{SPLIT}");
									}
									msg.text += text;
								}
								find_delete_target_t = true;
							}
						} else {
							msg.text += text;
						}
					} else {
						msg.text += text;
					}
				}
			}
			if (delete_target_t != 0 && find_delete_target_t == false) {
				msg.text.insert(target_index, "{SPLIT}");
				find_delete_target_t = true;
			}

			/**
			 * Clear audio buffer when the size exceeds iteration threshold or
			 * speech end is detected.
			 */
			if (pcmf32.size() > n_samples_iter_threshold * 0.66 || speech_has_end) {
				const auto t_now = Time::get_singleton()->get_ticks_msec();
				const auto t_diff = t_now - speech_to_text_obj->t_last_iter;
				speech_to_text_obj->t_last_iter = t_now;
				msg.is_partial = false;
				/**
				 * Keep the last few samples in the audio buffer, so the next
				 * iteration has a smoother start.
				 */
				if (delete_target_t == 0 || speech_has_end) {
					std::vector<float> last(pcmf32.end(), pcmf32.end());
					pcmf32 = std::move(last);
				} else {
					int target_index = int(delete_target_t / 100.0 * WHISPER_SAMPLE_RATE);
					if (target_index >= pcmf32.size()) {
						std::vector<float> last(pcmf32.end(), pcmf32.end());
						pcmf32 = std::move(last);
					} else {
						std::vector<float> last(pcmf32.begin() + target_index, pcmf32.end());
						pcmf32 = std::move(last);
					}
				}
			} else {
				msg.is_partial = true;
			}
			float time_end = Time::get_singleton()->get_ticks_msec() - time_started;
			speech_to_text_obj->s_mutex.lock();
			s_transcribed_msgs.insert(s_transcribed_msgs.end(), std::move(msg));
			std::vector<transcribed_msg> transcribed;
			transcribed = std::move(s_transcribed_msgs);
			s_transcribed_msgs.clear();

			Array ret;
			for (int i = 0; i < transcribed.size(); i++) {
				Dictionary cur_transcribed_msg;
				cur_transcribed_msg["is_partial"] = transcribed[i].is_partial;
				cur_transcribed_msg["text"] = String::utf8(transcribed[i].text.c_str());
				ret.push_back(cur_transcribed_msg);
			};
			speech_to_text_obj->call_deferred("emit_signal", "update_transcribed_msgs", time_end, ret);
			speech_to_text_obj->s_mutex.unlock();
		}
	}
}

void SpeechToText::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_audio_buffer", "buffer"), &SpeechToText::add_audio_buffer);
	ClassDB::bind_method(D_METHOD("get_entropy_threshold"), &SpeechToText::get_entropy_threshold);
	ClassDB::bind_method(D_METHOD("set_entropy_threshold", "entropy_threshold"), &SpeechToText::set_entropy_threshold);
	ClassDB::bind_method(D_METHOD("is_translate"), &SpeechToText::is_translate);
	ClassDB::bind_method(D_METHOD("set_translate", "translate"), &SpeechToText::set_translate);
	ClassDB::bind_method(D_METHOD("is_speed_up"), &SpeechToText::is_speed_up);
	ClassDB::bind_method(D_METHOD("set_speed_up", "speed_up"), &SpeechToText::set_speed_up);
	ClassDB::bind_method(D_METHOD("get_freq_thold"), &SpeechToText::get_freq_thold);
	ClassDB::bind_method(D_METHOD("set_freq_thold", "freq_thold"), &SpeechToText::set_freq_thold);
	ClassDB::bind_method(D_METHOD("get_vad_thold"), &SpeechToText::get_vad_thold);
	ClassDB::bind_method(D_METHOD("set_vad_thold", "vad_thold"), &SpeechToText::set_vad_thold);
	ClassDB::bind_method(D_METHOD("get_max_tokens"), &SpeechToText::get_max_tokens);
	ClassDB::bind_method(D_METHOD("set_max_tokens", "max_tokens"), &SpeechToText::set_max_tokens);
	ClassDB::bind_method(D_METHOD("get_n_threads"), &SpeechToText::get_n_threads);
	ClassDB::bind_method(D_METHOD("set_n_threads", "n_threads"), &SpeechToText::set_n_threads);

	ClassDB::bind_method(D_METHOD("get_language"), &SpeechToText::get_language);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &SpeechToText::set_language);
	ClassDB::bind_method(D_METHOD("get_language_model"), &SpeechToText::get_language_model);
	ClassDB::bind_method(D_METHOD("set_language_model", "model"), &SpeechToText::set_language_model);
	ClassDB::bind_method(D_METHOD("is_use_gpu"), &SpeechToText::is_use_gpu);
	ClassDB::bind_method(D_METHOD("set_use_gpu", "use_gpu"), &SpeechToText::set_use_gpu);
	ClassDB::bind_method(D_METHOD("start_listen"), &SpeechToText::start_listen);
	ClassDB::bind_method(D_METHOD("run"), &SpeechToText::run);
	ClassDB::bind_method(D_METHOD("stop_listen"), &SpeechToText::stop_listen);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "language", PROPERTY_HINT_ENUM, "Auto,English,Chinese,German,Spanish,Russian,Korean,French,Japanese,Portuguese,Turkish,Polish,Catalan,Dutch,Arabic,Swedish,Italian,Indonesian,Hindi,Finnish,Vietnamese,Hebrew,Ukrainian,Greek,Malay,Czech,Romanian,Danish,Hungarian,Tamil,Norwegian,Thai,Urdu,Croatian,Bulgarian,Lithuanian,Latin,Maori,Malayalam,Welsh,Slovak,Telugu,Persian,Latvian,Bengali,Serbian,Azerbaijani,Slovenian,Kannada,Estonian,Macedonian,Breton,Basque,Icelandic,Armenian,Nepali,Mongolian,Bosnian,Kazakh,Albanian,Swahili,Galician,Marathi,Punjabi,Sinhala,Khmer,Shona,Yoruba,Somali,Afrikaans,Occitan,Georgian,Belarusian,Tajik,Sindhi,Gujarati,Amharic,Yiddish,Lao,Uzbek,Faroese,Haitian_Creole,Pashto,Turkmen,Nynorsk,Maltese,Sanskrit,Luxembourgish,Myanmar,Tibetan,Tagalog,Malagasy,Assamese,Tatar,Hawaiian,Lingala,Hausa,Bashkir,Javanese,Sundanese,Cantonese"), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "language_model", PROPERTY_HINT_RESOURCE_TYPE, "WhisperResource"), "set_language_model", "get_language_model");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_gpu"), "set_use_gpu", "is_use_gpu");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "entropy_threshold"), "set_entropy_threshold", "get_entropy_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "translate"), "set_translate", "is_translate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "speed_up"), "set_speed_up", "is_speed_up");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "freq_thold"), "set_freq_thold", "get_freq_thold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vad_thold"), "set_vad_thold", "get_vad_thold");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tokens"), "set_max_tokens", "get_max_tokens");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "n_threads"), "set_n_threads", "get_n_threads");

	ADD_SIGNAL(MethodInfo("update_transcribed_msgs", PropertyInfo(Variant::INT, "process_time_ms"), PropertyInfo(Variant::ARRAY, "transcribed_msgs")));

	BIND_CONSTANT(SPEECH_SETTING_SAMPLE_RATE);
}
