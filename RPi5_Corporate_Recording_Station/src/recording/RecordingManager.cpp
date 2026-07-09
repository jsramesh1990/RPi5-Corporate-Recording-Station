#include "RecordingManager.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

RecordingManager::RecordingManager() 
    : video_recorder(std::make_unique<VideoRecorder>()),
      audio_recorder(std::make_unique<AudioRecorder>()) {
}

RecordingManager::~RecordingManager() {
    shutdown();
}

// ============================================
// INITIALIZATION
// ============================================

bool RecordingManager::initialize(const VideoRecorder::VideoConfig& video_config,
                                  const AudioRecorder::AudioConfig& audio_config) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        shutdown();
    }
    
    // Initialize video recorder
    if (!video_recorder->initialize(video_config)) {
        notifyError("Failed to initialize video recorder");
        return false;
    }
    
    // Initialize audio recorder
    if (!audio_recorder->initialize(audio_config)) {
        notifyError("Failed to initialize audio recorder");
        return false;
    }
    
    // Set up callbacks
    video_recorder->setOnProgress([this](float progress) {
        updateCombinedProgress(progress, audio_recorder->getStats().duration_seconds / 3600.0f * 100.0f);
    });
    
    audio_recorder->setOnProgress([this](float progress) {
        updateCombinedProgress(video_recorder->getStats().duration_seconds / 3600.0f * 100.0f, progress);
    });
    
    video_recorder->setOnError([this](const std::string& error) {
        notifyError("Video: " + error);
    });
    
    audio_recorder->setOnError([this](const std::string& error) {
        notifyError("Audio: " + error);
    });
    
    initialized = true;
    state = RecorderState::IDLE;
    
    std::cout << "Recording Manager initialized" << std::endl;
    return true;
}

void RecordingManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return;
    }
    
    if (state == RecorderState::RECORDING || state == RecorderState::PAUSED) {
        stopRecording();
    }
    
    video_recorder->shutdown();
    audio_recorder->shutdown();
    
    initialized = false;
    state = RecorderState::IDLE;
    
    std::cout << "Recording Manager shutdown" << std::endl;
}

// ============================================
// RECORDING CONTROL
// ============================================

bool RecordingManager::startRecording(const std::string& base_filename) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        notifyError("Not initialized");
        return false;
    }
    
    if (state == RecorderState::RECORDING) {
        notifyError("Already recording");
        return false;
    }
    
    // Generate filenames
    std::string timestamp = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    
    std::string prefix = base_filename.empty() ? "recording_" + timestamp : base_filename;
    std::string video_file = output_dir + "/" + prefix + ".mp4";
    std::string audio_file = output_dir + "/" + prefix + ".wav";
    std::string combined_file = output_dir + "/" + prefix + "_combined.mp4";
    
    // Start video recording
    if (!video_recorder->startRecording(video_file)) {
        notifyError("Failed to start video recording");
        return false;
    }
    
    // Start audio recording
    if (!audio_recorder->startRecording(audio_file)) {
        video_recorder->stopRecording();
        notifyError("Failed to start audio recording");
        return false;
    }
    
    // Update recording info
    current_recording.video_file = video_file;
    current_recording.audio_file = audio_file;
    current_recording.combined_file = combined_file;
    current_recording.start_time = std::chrono::system_clock::now();
    current_recording.has_video = true;
    current_recording.has_audio = true;
    current_recording.is_complete = false;
    
    state = RecorderState::RECORDING;
    notifyStateChange();
    notifyProgress(0.0f);
    
    std::cout << "Recording started: " << prefix << std::endl;
    return true;
}

bool RecordingManager::stopRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state != RecorderState::RECORDING && state != RecorderState::PAUSED) {
        return false;
    }
    
    // Stop video recording
    std::string video_file = video_recorder->stopRecording();
    
    // Stop audio recording
    std::string audio_file = audio_recorder->stopRecording();
    
    current_recording.stop_time = std::chrono::system_clock::now();
    current_recording.duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        current_recording.stop_time - current_recording.start_time).count();
    current_recording.is_complete = true;
    
    state = RecorderState::IDLE;
    notifyStateChange();
    notifyProgress(100.0f);
    
    // Auto-combine if enabled
    if (auto_combine && !video_file.empty() && !audio_file.empty()) {
        combineFiles(video_file, audio_file, current_recording.combined_file);
    }
    
    notifyComplete(current_recording);
    
    std::cout << "Recording stopped" << std::endl;
    return true;
}

void RecordingManager::pauseRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == RecorderState::RECORDING) {
        video_recorder->pauseRecording();
        audio_recorder->pauseRecording();
        state = RecorderState::PAUSED;
        notifyStateChange();
        std::cout << "Recording paused" << std::endl;
    }
}

void RecordingManager::resumeRecording() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (state == RecorderState::PAUSED) {
        video_recorder->resumeRecording();
        audio_recorder->resumeRecording();
        state = RecorderState::RECORDING;
        notifyStateChange();
        std::cout << "Recording resumed" << std::endl;
    }
}

// ============================================
// STATISTICS
// ============================================

RecordingManager::RecordingInfo RecordingManager::getCurrentRecording() const {
    std::lock_guard<std::mutex> lock(mutex);
    return current_recording;
}

VideoRecorder::RecordingStats RecordingManager::getVideoStats() const {
    return video_recorder->getStats();
}

AudioRecorder::RecordingStats RecordingManager::getAudioStats() const {
    return audio_recorder->getStats();
}

// ============================================
// PRIVATE METHODS
// ============================================

void RecordingManager::setState(RecorderState new_state) {
    if (state != new_state) {
        state = new_state;
        notifyStateChange();
    }
}

void RecordingManager::notifyStateChange() {
    if (on_state_change) {
        on_state_change(state);
    }
}

void RecordingManager::notifyProgress(float progress) {
    if (on_progress) {
        on_progress(progress);
    }
}

void RecordingManager::notifyComplete(const RecordingInfo& info) {
    if (on_complete) {
        on_complete(info);
    }
}

void RecordingManager::notifyError(const std::string& error) {
    if (on_error) {
        on_error(error);
    }
    std::cerr << "Recording Manager Error: " << error << std::endl;
}

bool RecordingManager::combineFiles(const std::string& video_file, 
                                   const std::string& audio_file,
                                   const std::string& output_file) {
    // Use FFmpeg to combine video and audio
    std::string cmd = "ffmpeg -i " + video_file + " -i " + audio_file + 
                     " -c:v copy -c:a aac -map 0:v:0 -map 1:a:0 " + output_file + 
                     " 2>/dev/null";
    
    int result = system(cmd.c_str());
    if (result != 0) {
        notifyError("Failed to combine video and audio");
        return false;
    }
    
    std::cout << "Combined file created: " << output_file << std::endl;
    return true;
}

void RecordingManager::updateCombinedProgress(float video_progress, float audio_progress) {
    float combined = (video_progress + audio_progress) / 2.0f;
    notifyProgress(combined);
}

// ============================================
// CALLBACKS
// ============================================

void RecordingManager::setOnStateChange(StateCallback callback) {
    on_state_change = callback;
}

void RecordingManager::setOnProgress(ProgressCallback callback) {
    on_progress = callback;
}

void RecordingManager::setOnComplete(CompleteCallback callback) {
    on_complete = callback;
}

void RecordingManager::setOnError(ErrorCallback callback) {
    on_error = callback;
}
