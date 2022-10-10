/* Webcamoid, webcam capture application.
 * Copyright (C) 2017  Gonzalo Exequiel Pedone
 *
 * Webcamoid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Webcamoid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Webcamoid. If not, see <http://www.gnu.org/licenses/>.
 *
 * Web-Site: http://webcamoid.github.io/
 */

/******************************************************************************
Modifications Copyright (C) 2020 by Streamlabs (General Workings Inc)

04/20/2020: Rewrite partial and sometimes complete to fits the application needs

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#include <CoreMediaIO/CMIOSampleBuffer.h>

#include "stream.hpp"
#include "clock.hpp"
#include "util.h"
#include "logger.h"

#include <mutex>
#include <thread>
#include <random>

Stream::Stream(bool registerObject, Object *parent) : Object(parent)
{
	this->m_className = "Stream";
	this->m_classID = kCMIOStreamClassID;

	this->m_clock = std::make_shared<Clock>("CMIO::VirtualCamera::Stream", CMTimeMake(1, 10), 100, 10);

	CMSimpleQueueCreate(kCFAllocatorDefault, 30, &this->m_queue);

	if (registerObject) {
		this->createObject();
		this->registerObject();
	}

	this->m_properties.setProperty(kCMIOStreamPropertyClock, this->m_clock);
}

Stream::~Stream()
{
	this->registerObject(false);
}

OSStatus Stream::createObject()
{
	PrintFunction();

	if (!this->m_pluginInterface || !*this->m_pluginInterface || !this->m_parent)
		return kCMIOHardwareUnspecifiedError;

	CMIOObjectID streamID = 0;

	auto status = CMIOObjectCreate(this->m_pluginInterface, this->m_parent->objectID(), this->m_classID, &streamID);

	if (status == kCMIOHardwareNoError) {
		this->m_isCreated = true;
		this->m_objectID = streamID;
	}

	return status;
}

OSStatus Stream::registerObject(bool regist)
{
	PrintFunction();
	OSStatus status = kCMIOHardwareUnspecifiedError;

	if (!this->m_isCreated || !this->m_pluginInterface || !*this->m_pluginInterface || !this->m_parent)
		return status;

	if (regist) {
		status = CMIOObjectsPublishedAndDied(this->m_pluginInterface, this->m_parent->objectID(), 1, &this->m_objectID, 0, nullptr);
	} else {
		status = CMIOObjectsPublishedAndDied(this->m_pluginInterface, this->m_parent->objectID(), 0, nullptr, 1, &this->m_objectID);
	}

	return status;
}

void Stream::setFrameInfo(const FrameInfo &fi)
{
	PrintFunction();

	this->m_properties.setProperty(kCMIOStreamPropertyFormatDescription, fi);
}

void Stream::setFPS(const double &fps)
{
	this->m_properties.setProperty(kCMIOStreamPropertyFrameRate, fps);
}

bool Stream::start()
{
	PrintFunction();

	if (this->m_running)
		return false;

	this->m_sequence = 0;
	memset(&this->m_pts, 0, sizeof(CMTime));
	this->m_running = this->startTimer();

	return this->m_running;
}

void Stream::stop()
{
	PrintFunction();

	if (!this->m_running)
		return;

	this->m_running = false;
	this->stopTimer();
}

bool Stream::running()
{
	return this->m_running;
}

void Stream::frameReady(const uint8_t *data)
{
	PrintFunction();

	Print("Stream::frameReady - 0");
	if (!this->m_running)
		return;

	Print("Stream::frameReady - 1");
	if (!data)
		Print("Stream::frameReady Null data");

	this->m_mutex.lock();
	Print("Stream::frameReady - 2");
	this->m_currentData = data;
	Print("Stream::frameReady - 3");
	this->m_mutex.unlock();
	Print("Stream::frameReady - 4");
}
void Stream::setMirror(bool horizontalMirror, bool verticalMirror)
{
	PrintFunction();
	if (this->m_horizontalMirror == horizontalMirror && this->m_verticalMirror == verticalMirror)
		return;

	this->m_horizontalMirror = horizontalMirror;
	this->m_verticalMirror = verticalMirror;
}

OSStatus Stream::copyBufferQueue(CMIODeviceStreamQueueAlteredProc queueAlteredProc, void *queueAlteredRefCon, CMSimpleQueueRef *queue)
{
	PrintFunction();

	this->m_queueAltered = queueAlteredProc;
	this->m_queueAlteredRefCon = queueAlteredRefCon;
	*queue = queueAlteredProc ? this->m_queue : nullptr;

	if (*queue)
		CFRetain(*queue);

	return kCMIOHardwareNoError;
}

OSStatus Stream::deckPlay()
{
	return kCMIOHardwareUnspecifiedError;
}

OSStatus Stream::deckStop()
{
	return kCMIOHardwareUnspecifiedError;
}

OSStatus Stream::deckJog(SInt32 speed)
{
	return kCMIOHardwareUnspecifiedError;
}

OSStatus Stream::deckCueTo(Float64 frameNumber, Boolean playOnCue)
{
	return kCMIOHardwareUnspecifiedError;
}

void Stream::renderFrames()
{
	while (!stop_timer) {
		this->m_mutex.lock();
		sendFrame(this->m_currentData);
		this->m_mutex.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

bool Stream::startTimer()
{

	// if (this->m_timer)
	//     return false;

	// Float64 fps = 0;
	// this->m_properties.getProperty(kCMIOStreamPropertyFrameRate, &fps);

	// CFTimeInterval interval = 1.0 / 60.0;
	// CFRunLoopTimerContext context {0, this, nullptr, nullptr, nullptr};
	// this->m_timer =
	//         CFRunLoopTimerCreate(kCFAllocatorDefault,
	//                              0.0,
	//                              interval,
	//                              0,
	//                              0,
	//                              Stream::streamLoop,
	//                              &context);

	// if (!this->m_timer)
	//     return false;

	// Print("start timmer now");
	// CFRunLoopAddTimer(CFRunLoopGetMain(),
	//                   this->m_timer,
	//                   kCFRunLoopCommonModes);

	// Print("start timmer now - end");

	stop_timer = false;
	timer = new std::thread(&Stream::renderFrames, this);

	return true;
}

void Stream::stopTimer()
{
	// PrintFunction();
	// if (!this->m_timer)
	//     return;

	// CFRunLoopTimerInvalidate(this->m_timer);
	// CFRunLoopRemoveTimer(CFRunLoopGetMain(),
	//                      this->m_timer,
	//                      kCFRunLoopCommonModes);
	// CFRelease(this->m_timer);
	// this->m_timer = nullptr;

	stop_timer = true;
	if (timer->joinable())
		timer->join();
}

void Stream::streamLoop(CFRunLoopTimerRef timer, void *info)
{
	auto self = reinterpret_cast<Stream *>(info);

	Print("Stream::streamLoop");
	Print("Stream::streamLoop, is Running: ", self->m_running);

	if (!self->m_running)
		return;

	self->m_mutex.lock();
	self->sendFrame(self->m_currentData);
	self->m_mutex.unlock();
	Print("Stream::streamLoop - end");
}

void doHorrizontalMirror(uint32_t height, uint32_t width, const uint8_t *source, uint8_t *dest)
{
	int linesize = width * 2;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < linesize; x += 4) {
			dest[y * linesize + x] = source[y * linesize + linesize - x];
			dest[y * linesize + x + 1] = source[y * linesize + linesize - x - 1];
			dest[y * linesize + x + 2] = source[y * linesize + linesize - x - 2];
			dest[y * linesize + x + 3] = source[y * linesize + linesize - x - 3];
		}
	}
}

void Stream::sendFrame(const uint8_t *data)
{
	Print("Stream::sendFrame");
	Print("Stream::sendFrame - 0");
	if (CMSimpleQueueGetFullness(this->m_queue) >= 1.0f)
		return;

	Print("Stream::sendFrame - 1");
	bool resync = false;
	auto hostTime = CFAbsoluteTimeGetCurrent();
	auto pts = CMTimeMake(int64_t(hostTime), 1e9);
	auto ptsDiff = CMTimeGetSeconds(CMTimeSubtract(this->m_pts, pts));

	Print("Stream::sendFrame - 2");
	if (CMTimeCompare(pts, this->m_pts) == 0)
		return;

	Print("Stream::sendFrame - 3");
	FrameInfo fi;
	Float64 fps;

	this->m_properties.getProperty(kCMIOStreamPropertyFormatDescription, &fi);
	this->m_properties.getProperty(kCMIOStreamPropertyFrameRate, &fps);

	if (CMTIME_IS_INVALID(this->m_pts) || ptsDiff < 0 || ptsDiff > 2. / fps) {
		this->m_pts = pts;
		resync = true;
	}

	CMIOStreamClockPostTimingEvent(this->m_pts, UInt64(hostTime), resync, this->m_clock->ref());

	CVImageBufferRef imageBuffer = nullptr;
	CVPixelBufferCreate(kCFAllocatorDefault, fi.width, fi.height, fi.pix_format, nullptr, &imageBuffer);

	Print("Stream::sendFrame, fps: ", fps);
	Print("Stream::sendFrame, fi.width: ", fi.width);
	Print("Stream::sendFrame, fi.height: ", fi.height);
	Print("Stream::sendFrame, fi.pix_format: ", fi.pix_format);

	Print("Stream::sendFrame - 4");
	if (!imageBuffer)
		return;

	Print("Stream::sendFrame - 5");
	CVPixelBufferLockBaseAddress(imageBuffer, 0);
	auto data2 = CVPixelBufferGetBaseAddress(imageBuffer);
	if (!data || !data2) // No valid data to render
		return;
	Print("Stream::sendFrame - 6");

	if (this->m_horizontalMirror)
		doHorrizontalMirror(fi.height, fi.width, data, (uint8_t *)data2);
	else
		memcpy(data2, data, fi.width * fi.height * BYTES_PER_PIXEL);

	CVPixelBufferUnlockBaseAddress(imageBuffer, 0);

	CMVideoFormatDescriptionRef format = nullptr;
	CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, imageBuffer, &format);

	auto duration = CMTimeMake(1e3, int32_t(1e3 * fps));
	CMSampleTimingInfo timingInfo{duration, this->m_pts, this->m_pts};

	Print("Stream::sendFrame - 7");
	CMSampleBufferRef buffer = nullptr;
	CMIOSampleBufferCreateForImageBuffer(kCFAllocatorDefault, imageBuffer, format, &timingInfo, this->m_sequence,
					     resync ? kCMIOSampleBufferDiscontinuityFlag_UnknownDiscontinuity : kCMIOSampleBufferNoDiscontinuities, &buffer);
	CFRelease(format);
	CFRelease(imageBuffer);

	CMSimpleQueueEnqueue(this->m_queue, buffer);
	this->m_pts = CMTimeAdd(this->m_pts, duration);
	this->m_sequence++;
	Print("Stream::sendFrame - 8");

	if (this->m_queueAltered)
		this->m_queueAltered(this->m_objectID, buffer, this->m_queueAlteredRefCon);
	Print("Stream::sendFrame end");
}