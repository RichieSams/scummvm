/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
 * This code is based on Labyrinth of Time code with assistance of
 *
 * Copyright (c) 1993 Terra Nova Development
 * Copyright (c) 2004 The Wyrmkeep Entertainment Co.
 *
 */

#include "audio/decoders/raw.h"

#include "lab/lab.h"

#include "lab/anim.h"
#include "lab/eventman.h"
#include "lab/music.h"
#include "lab/resource.h"

namespace Lab {

#define SAMPLESPEED       15000
#define CLOWNROOM           123
#define DIMROOM              80

Music::Music(LabEngine *vm) : _vm(vm) {
	_musicFile = nullptr;
	_musicPaused = false;
	_curRoomMusic = 1;
	_storedPos = 0;
}

byte Music::getSoundFlags() {
	byte soundFlags = Audio::FLAG_LITTLE_ENDIAN;
	if (_vm->getPlatform() == Common::kPlatformWindows)
		soundFlags |= Audio::FLAG_16BITS;
	else if (_vm->getPlatform() == Common::kPlatformDOS)
		soundFlags |= Audio::FLAG_UNSIGNED;

	return soundFlags;
}

void Music::changeMusic(const Common::String filename, bool storeCurPos, bool seektoStoredPos) {
	if (storeCurPos)
		_storedPos = _musicFile->pos();

	_musicPaused = false;
	stopSoundEffect();
	freeMusic();
	_musicFile = _vm->_resource->openDataFile(filename);
	if (seektoStoredPos)
		_musicFile->seek(_storedPos);

	Audio::SeekableAudioStream *audioStream = Audio::makeRawStream(_musicFile, SAMPLESPEED, getSoundFlags());
	Audio::LoopingAudioStream *loopingAudioStream = new Audio::LoopingAudioStream(audioStream, 0);
	_vm->_mixer->playStream(Audio::Mixer::kMusicSoundType, &_musicHandle, loopingAudioStream);
}

void Music::playSoundEffect(uint16 sampleSpeed, uint32 length, bool loop, Common::File *dataFile) {
	pauseBackMusic();
	stopSoundEffect();

	if (sampleSpeed < 4000)
		sampleSpeed = 4000;

	// NOTE: We need to use malloc(), cause this will be freed with free()
	// by the music code
	byte *soundData = (byte *)malloc(length);
	dataFile->read(soundData, length);

	Audio::SeekableAudioStream *audioStream = Audio::makeRawStream((const byte *)soundData, length, sampleSpeed, getSoundFlags());
	uint loops = (loop) ? 0 : 1;
	Audio::LoopingAudioStream *loopingAudioStream = new Audio::LoopingAudioStream(audioStream, loops);
	_vm->_mixer->playStream(Audio::Mixer::kSFXSoundType, &_sfxHandle, loopingAudioStream);
}

void Music::stopSoundEffect() {
	if (isSoundEffectActive())
		_vm->_mixer->stopHandle(_sfxHandle);
}

bool Music::isSoundEffectActive() const {
	return _vm->_mixer->isSoundHandleActive(_sfxHandle);
}

void Music::freeMusic() {
	_vm->_mixer->stopHandle(_musicHandle);
	_vm->_mixer->stopHandle(_sfxHandle);
	_musicPaused = false;
	_musicFile = nullptr;
}

void Music::pauseBackMusic() {
	if (!_musicPaused) {
		stopSoundEffect();
		_vm->_mixer->pauseHandle(_musicHandle, true);
		_musicPaused = true;
	}
}

void Music::resumeBackMusic() {
	if (_musicPaused) {
		stopSoundEffect();
		_vm->_mixer->pauseHandle(_musicHandle, false);
		_musicPaused = false;
	}
}

void Music::checkRoomMusic() {
	if ((_curRoomMusic == _vm->_roomNum) || !_musicFile)
		return;

	if (_vm->_roomNum == CLOWNROOM) {
		changeMusic("Music:Laugh", true, false);
	} else if (_vm->_roomNum == DIMROOM) {
		changeMusic("Music:Rm81", true, false);
	} else if (_curRoomMusic == CLOWNROOM || _curRoomMusic == DIMROOM) {
		if (_vm->getPlatform() != Common::kPlatformAmiga)
			changeMusic("Music:Backgrou", false, true);
		else
			changeMusic("Music:Background", false, true);
	}

	_curRoomMusic = _vm->_roomNum;
}

bool Music::loadSoundEffect(const Common::String filename, bool loop, bool waitTillFinished) {
	Common::File *file = _vm->_resource->openDataFile(filename, MKTAG('D', 'I', 'F', 'F'));
	stopSoundEffect();

	if (!file)
		return false;

	_vm->_anim->_doBlack = false;
	readSound(waitTillFinished, loop, file);

	return true;
}

void Music::readSound(bool waitTillFinished, bool loop, Common::File *file) {
	uint32 magicBytes = file->readUint32LE();
	if (magicBytes != 1219009121) {
		warning("readSound: Bad signature, skipping");
		return;
	}
	uint32 soundTag = file->readUint32LE();
	uint32 soundSize = file->readUint32LE();

	if (soundTag == 0)
		file->skip(soundSize);	// skip the header
	else
		return;

	while (soundTag != 65535) {
		_vm->updateEvents();
		soundTag = file->readUint32LE();
		soundSize = file->readUint32LE() - 8;

		if ((soundTag == 30) || (soundTag == 31)) {
			if (waitTillFinished) {
				while (isSoundEffectActive()) {
					_vm->updateEvents();
					_vm->waitTOF();
				}
			}

			file->skip(4);

			uint16 sampleRate = file->readUint16LE();
			file->skip(2);
			playSoundEffect(sampleRate, soundSize, loop, file);
		} else if (soundTag == 65535) {
			if (waitTillFinished) {
				while (isSoundEffectActive()) {
					_vm->updateEvents();
					_vm->waitTOF();
				}
			}
		} else
			file->skip(soundSize);
	}
}

} // End of namespace Lab