﻿/*
* MIT License
*
* Copyright (c) 2016 xiongziliang <771730766@qq.com>
*
* This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "RtmpMuxer.h"

namespace mediakit {

void RtmpMuxer::addTrack(const Track::Ptr &track) {
    //记录该Track
    auto codec_id = track->getCodecId();
    _track_map[codec_id] = track;

    auto lam = [this,track](){
        //异步生成Rtmp编码器
        auto encoder = Factory::getRtmpCodecByTrack(track);
        if (!encoder) {
            return;
        }

        //根据track生产metedata
        Metedata::Ptr metedate;
        switch (track->getTrackType()){
            case TrackVideo:{
                metedate = std::make_shared<VideoMete>(dynamic_pointer_cast<VideoTrack>(track));
            }
                break;
            case TrackAudio:{
                metedate = std::make_shared<AudioMete>(dynamic_pointer_cast<AudioTrack>(track));
            }
                break;
            default:
                return;;

        }
        //添加其metedata
        metedate->getMetedata().object_for_each([&](const std::string &key, const AMFValue &value){
            _metedata.set(key,value);
        });
        //设置Track的代理，这样输入frame至Track时，最终数据将输出到RtmpEncoder中
        track->addDelegate(encoder);
        //Rtmp编码器共用同一个环形缓存
        encoder->setRtmpRing(_rtmpRing);
    };
    if(track->ready()){
        lam();
    }else{
        _trackReadyCallback[codec_id] = lam;
    }
}


const AMFValue &RtmpMuxer::getMetedata() const {
    if(!_trackReadyCallback.empty()){
        //尚未就绪
        static AMFValue s_amf;
        return s_amf;
    }
    return _metedata;
}


void RtmpMuxer::inputFrame(const Frame::Ptr &frame) {
    auto codec_id = frame->getCodecId();
    auto it = _track_map.find(codec_id);
    if (it == _track_map.end()) {
        return;
    }
    it->second->inputFrame(frame);
    if(!_trackReadyCallback.empty() && it->second->ready()){
        //Track由未就绪状态装换成就绪状态，我们就生成metedata以及Rtmp编码器
        auto it_callback = _trackReadyCallback.find(codec_id);
        if(it_callback != _trackReadyCallback.end()){
            it_callback->second();
            _trackReadyCallback.erase(it_callback);
        }
    }

    if(!_inited && _trackReadyCallback.empty()){
        _inited = true;
        onInited();
    }
}

bool RtmpMuxer::inputRtmp(const RtmpPacket::Ptr &rtmp , bool key_pos) {
    _rtmpRing->write(rtmp,key_pos);
    return key_pos;
}

RtmpRingInterface::RingType::Ptr RtmpMuxer::getRtmpRing() const {
    return _rtmpRing;
}

}