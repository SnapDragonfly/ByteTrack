#include "Tracker.h"
#include "BYTETracker.h"
#include <fstream>

NvMOTContext::NvMOTContext(const NvMOTConfig &configIn, NvMOTConfigResponse &configResponse) {
    configResponse.summaryStatus = NvMOTConfigStatus_OK;
}

NvMOTStatus NvMOTContext::processFrame(const NvMOTProcessParams *params, NvMOTTrackedObjBatch *pTrackedObjectsBatch) {
    for (uint streamIdx = 0; streamIdx < pTrackedObjectsBatch->numFilled; streamIdx++){
        NvMOTTrackedObjList   *trackedObjList = &pTrackedObjectsBatch->list[streamIdx];
        NvMOTFrame            *frame          = &params->frameList[streamIdx];
        std::vector<NvObject> nvObjects(frame->objectsIn.numFilled);
        for (uint32_t numObjects = 0; numObjects < frame->objectsIn.numFilled; numObjects++) {
            NvMOTObjToTrack *objectToTrack = &frame->objectsIn.list[numObjects];
            NvObject nvObject;
            nvObject.prob    = objectToTrack->confidence;
            nvObject.label   = objectToTrack->classId;
            nvObject.rect[0] = objectToTrack->bbox.x;
            nvObject.rect[1] = objectToTrack->bbox.y;
            nvObject.rect[2] = objectToTrack->bbox.width;
            nvObject.rect[3] = objectToTrack->bbox.height;
            nvObject.associatedObjectIn = objectToTrack;
            nvObjects.push_back(nvObject);
        }

        if (byteTrackerMap.find(frame->streamID) == byteTrackerMap.end())
            byteTrackerMap.insert(std::pair<uint64_t, std::shared_ptr<BYTETracker>>(frame->streamID, std::make_shared<BYTETracker>(15, 30)));

        std::vector<STrack> outputTracks = byteTrackerMap.at(frame->streamID)->update(nvObjects);

        NvMOTTrackedObj *trackedObjs = new NvMOTTrackedObj[512];
        int             filled       = 0;

        for (STrack &sTrack: outputTracks) {
            std::vector<float> tlwh        = sTrack.original_tlwh;
            NvMOTRect          motRect{tlwh[0], tlwh[1], tlwh[2], tlwh[3]};
            NvMOTTrackedObj    *trackedObj             = new NvMOTTrackedObj;
            trackedObj->classId                        = sTrack.label;
            trackedObj->trackingId                     = (uint64_t) sTrack.track_id;
            trackedObj->bbox                           = motRect;
            trackedObj->confidence                     = 1;
            trackedObj->age                            = (uint32_t) sTrack.tracklet_len;
            trackedObj->associatedObjectIn             = sTrack.associatedObjectIn;
            trackedObj->associatedObjectIn->doTracking = true;
            trackedObjs[filled++]                      = *trackedObj;
        }

        trackedObjList->streamID     = frame->streamID;
        trackedObjList->frameNum     = frame->frameNum;
        trackedObjList->valid        = true;
        trackedObjList->list         = trackedObjs;
        trackedObjList->numFilled    = filled;
        trackedObjList->numAllocated = 512;
    }
}

#if defined(NVDS_VERSION_MAJOR) && defined(NVDS_VERSION_MINOR)
#if NVDS_VERSION_MAJOR == 7 && NVDS_VERSION_MINOR == 1
    #pragma message("API no required for DeepStream 7.1")
#elif NVDS_VERSION_MAJOR == 6 && NVDS_VERSION_MINOR == 3
NvMOTStatus NvMOTContext::processFramePast(const NvMOTProcessParams *params,
                                           NvDsPastFrameObjBatch *pPastFrameObjectsBatch) {
    return NvMOTStatus_OK;
}
#else
    #error "Unsupported DeepStream version"
#endif
#else
    #error "NVDS_VERSION_MAJOR or NVDS_VERSION_MINOR not defined"
#endif


NvMOTStatus NvMOTContext::removeStream(const NvMOTStreamId streamIdMask) {
    if (byteTrackerMap.find(streamIdMask) != byteTrackerMap.end()){
        std::cout << "Removing tracker for stream: " << streamIdMask << std::endl;
        byteTrackerMap.erase(streamIdMask);
    }
    return NvMOTStatus_OK;
}


NvMOTStatus NvMOTContext::retrieveMiscData(const NvMOTProcessParams *params,
                                             NvMOTTrackerMiscData *pTrackerMiscData)
{
	return NvMOTStatus_OK;
}

