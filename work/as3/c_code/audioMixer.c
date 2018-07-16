// Incomplete implementation of an audio mixer. Search for "REVISIT" to find things
// which are left as incomplete.
// Note: Generates low latency audio on BeagleBone Black; higher latency found on host.
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h> // needed for mixer

#include "audioMixer.h"

static snd_pcm_t *handle;

#define DRUM_BD_FILE "beatbox-wav-files/100051__menegass__gui-drum-bd-hard.wav"
#define DRUM_HH_FILE "beatbox-wav-files/100053__menegass__gui-drum-cc.wav"
#define DRUM_SNR_FILE "beatbox-wav-files/100058__menegass__gui-drum-snare-hard.wav"

#define DEFAULT_VOLUME 80
#define DEFAULT_BPM 120

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
#define SAMPLE_SIZE (sizeof(short)) 			// bytes per sample
// Sample size note: This works for mono files because each sample ("frame') is 1 value.
// If using stereo files then a frame would be two samples.

static unsigned long playbackBufferSize;
static short *playbackBuffer = NULL;

// Currently active (waiting to be played) sound bites
#define MAX_SOUND_BITES 30

#define SECONDS_TO_NS 1000000000

typedef struct {
	// A pointer to a previously allocated sound bite (wavedata_t struct).
	// Note that many different sound-bite slots could share the same pointer
	// (overlapping cymbal crashes, for example)
	wavedata_t *pSound;

	// The offset into the pData of pSound. Indicates how much of the
	// sound has already been played (and hence where to start playing next).
	int location;
} playbackSound_t;

static playbackSound_t soundBites[MAX_SOUND_BITES];

// Playback threading
void* playbackThread(void* arg);
void* drumBeatThread(void* arg);
static _Bool stopping = false;
static pthread_t playbackThreadId;
static pthread_t drumBeatThreadId;
static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;

// internal control variables
static int volume = 0;
static int beatType = 1;
static int bpm = 0;
static int eighthNWaitTime = 0;

void AudioMixer_init(void)
{
	AudioMixer_setVolume(DEFAULT_VOLUME);
	AudioMixer_setBPM(DEFAULT_BPM);

	// Initialize the currently active sound-bites being played
	// REVISIT:- Implement this. Hint: set the pSound pointer to NULL for each
	//     sound bite.
	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		soundBites[i].pSound = NULL;
		soundBites[i].location = 0;
	}

	// Open the PCM output
	int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Configure parameters of PCM output
	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			50000);		// 0.05 seconds per buffer
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Allocate this software's playback buffer to be the same size as the
	// the hardware's playback buffers for efficient data transfers.
	// ..get info on the hardware buffers:
 	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	// ..allocate playback buffer:
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));

	AudioMixer_readWaveFileIntoMemory(DRUM_BD_FILE, &bassDrum);
	AudioMixer_readWaveFileIntoMemory(DRUM_HH_FILE, &hiHat);
	AudioMixer_readWaveFileIntoMemory(DRUM_SNR_FILE, &snareDrum);

	// Launch playback thread:
	pthread_create(&playbackThreadId, NULL, playbackThread, NULL);
	pthread_create(&drumBeatThreadId, NULL, drumBeatThread, NULL);
}


// Client code must call AudioMixer_freeWaveFileData to free dynamically allocated data.
void AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound)
{
	assert(pSound);

	// The PCM data in a wave file starts after the header:
	const int PCM_DATA_OFFSET = 44;

	// Open the wave file
	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		fprintf(stderr, "ERROR: Unable to open file %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	int sizeInBytes = ftell(file) - PCM_DATA_OFFSET;
	pSound->numSamples = sizeInBytes / SAMPLE_SIZE;

	// Search to the start of the data in the file
	fseek(file, PCM_DATA_OFFSET, SEEK_SET);

	// Allocate space to hold all PCM data
	pSound->pData = malloc(sizeInBytes);
	if (pSound->pData == 0) {
		fprintf(stderr, "ERROR: Unable to allocate %d bytes for file %s.\n",
				sizeInBytes, fileName);
		exit(EXIT_FAILURE);
	}

	// Read PCM data from wave file into memory
	int samplesRead = fread(pSound->pData, SAMPLE_SIZE, pSound->numSamples, file);
	if (samplesRead != pSound->numSamples) {
		fprintf(stderr, "ERROR: Unable to read %d samples from file %s (read %d).\n",
				pSound->numSamples, fileName, samplesRead);
		exit(EXIT_FAILURE);
	}
}

void AudioMixer_freeWaveFileData(wavedata_t *pSound)
{
	pSound->numSamples = 0;
	free(pSound->pData);
	pSound->pData = NULL;
}

void AudioMixer_queueSound(wavedata_t *pSound)
{
	// Ensure we are only being asked to play "good" sounds:
	assert(pSound->numSamples > 0);
	assert(pSound->pData);

	int i = 0;
	pthread_mutex_lock(&audioMutex);
	for (i = 0; i < MAX_SOUND_BITES; i++) {
		if (soundBites[i].pSound == NULL) {
			soundBites[i].pSound = pSound;
			soundBites[i].location = 0;
			break;
		}
	}
	pthread_mutex_unlock(&audioMutex);

	if (i == MAX_SOUND_BITES) {
		printf("Error adding sound to queue\n");
		return;
	}
}

void AudioMixer_cleanup(void)
{
	printf("Stopping audio...\n");

	// Stop the PCM generation thread
	stopping = true;
	pthread_join(playbackThreadId, NULL);
	pthread_join(drumBeatThreadId, NULL);

	// Shutdown the PCM output, allowing any pending sound to play out (drain)
	snd_pcm_drain(handle);
	snd_pcm_close(handle);

	// Free playback buffer
	// (note that any wave files read into wavedata_t records must be freed
	//  in addition to this by calling AudioMixer_freeWaveFileData() on that struct.)
	free(playbackBuffer);
	playbackBuffer = NULL;

	AudioMixer_freeWaveFileData(&bassDrum);
	AudioMixer_freeWaveFileData(&hiHat);
	AudioMixer_freeWaveFileData(&snareDrum);

	printf("Done stopping audio...\n");
	fflush(stdout);
}


int AudioMixer_getVolume()
{
	// Return the cached volume; good enough unless someone is changing
	// the volume through other means and the cached value is out of date.
	return volume;
}

// Function copied from:
// http://stackoverflow.com/questions/6787318/set-alsa-master-volume-from-c-code
// Written by user "trenki".
void AudioMixer_setVolume(int newVolume)
{
	// Ensure volume is reasonable; If so, cache it for later getVolume() calls.
	if (newVolume < 0 || newVolume > AUDIOMIXER_MAX_VOLUME) {
		printf("ERROR: Volume must be between 0 and 100.\n");
		return;
	}
	volume = newVolume;

    long min, max;
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    const char *selem_name = "PCM";

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);

    snd_mixer_close(handle);
}


// Fill the playbackBuffer array with new PCM values to output.
//    playbackBuffer: buffer to fill with new PCM data from sound bites.
//    size: the number of values to store into playbackBuffer
static void fillPlaybackBuffer(short *playbackBuffer, int size)
{
	int i, j, currLocation;
	int result = 0;

	// holds up to 30 sound bite indexes which should be added to be played
	int toPlay[30] = {0};
	int counter = 0; // number of current sound bites to play

	memset(playbackBuffer, 0, playbackBufferSize);

	// Find which sounds should be played for this iteration of playbackBuffer
	// This should be protected by the Mutex
	pthread_mutex_lock(&audioMutex);
	for (i = 0; i < MAX_SOUND_BITES; i++) {
		if (soundBites[i].pSound != NULL) {
			toPlay[counter] = i;
			counter++;
		}
	}
	pthread_mutex_unlock(&audioMutex);
	for (i = 0; i < playbackBufferSize; i++) {
		result = 0;
		for (j = 0; j < counter; j++) {
			currLocation = soundBites[toPlay[j]].location;
			if (currLocation < soundBites[toPlay[j]].pSound->numSamples) {
				result += soundBites[toPlay[j]].pSound->pData[currLocation];
				soundBites[toPlay[j]].location++;
			}
		}
		if (result > SHRT_MAX) {
			result = SHRT_MAX;
		} else if (result < SHRT_MIN) {
			result = SHRT_MIN;
		}

		playbackBuffer[i] = (short)result;
	}

	for (i = 0; i < counter; i++) {
		if (soundBites[toPlay[i]].location >= soundBites[toPlay[i]].pSound->numSamples) {
			soundBites[toPlay[i]].pSound = NULL;
			soundBites[toPlay[i]].location = 0;
		}
	}
}

// returns the nanoseconds of wait time for an EIGHTH NOTE based on bpm
static int bpmToWaitTime(int bpm) {
	return (60.0/bpm/2)*SECONDS_TO_NS; //this is from description of bpm from the assignment
}

int AudioMixer_getBPM() {
	return bpm;
}

void AudioMixer_setBPM(int newBPM) {
	if (newBPM < AUDIOMIXER_MIN_BPM || newBPM > AUDIOMIXER_MAX_BPM) {
		printf("ERROR: BPM must be between 40 and 300.\n");
		return;
	}

	bpm = newBPM;
	eighthNWaitTime = bpmToWaitTime(bpm);
}

int AudioMixer_getBeat() {
	return beatType;
}

void AudioMixer_setBeat(int newBeat) {
	if (newBeat < 0 || newBeat > 2) {
		printf("ERROR: Beat must be between 0 and 2.\n");
		return;
	}
	beatType = newBeat;
}

void* playbackThread(void* arg)
{
	while (!stopping) {
		// Generate next block of audio
		fillPlaybackBuffer(playbackBuffer, playbackBufferSize);
		// Output the audio
		snd_pcm_sframes_t frames = snd_pcm_writei(handle,
				playbackBuffer, playbackBufferSize);

		// Check for (and handle) possible error conditions on output
		if (frames < 0) {
			fprintf(stderr, "AudioMixer: writei() returned %li\n", frames);
			frames = snd_pcm_recover(handle, frames, 1);
		}
		if (frames < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n",
					frames);
			exit(EXIT_FAILURE);
		}
		if (frames > 0 && frames < playbackBufferSize) {
			printf("Short write (expected %li, wrote %li)\n",
					playbackBufferSize, frames);
		}
	}

	return NULL;
}

void* drumBeatThread(void* arg) {
	while (!stopping) {
		if (beatType == 0) {

		} else if (beatType == 1) {
			AudioMixer_queueSound(&bassDrum);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&snareDrum);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
		} else if (beatType == 2) {
			AudioMixer_queueSound(&bassDrum);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime/2}}, NULL);
			AudioMixer_queueSound(&bassDrum);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime/2}}, NULL);
			AudioMixer_queueSound(&snareDrum);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&bassDrum);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&bassDrum);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&snareDrum);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
			AudioMixer_queueSound(&hiHat);
			nanosleep((const struct timespec[]){{0, eighthNWaitTime}}, NULL);
		}
	}

	return NULL;
}