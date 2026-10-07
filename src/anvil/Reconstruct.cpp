// Reconstruct.cpp — deterministic estimation + confidence-weighted accumulate.
#include "Reconstruct.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace anvil {

namespace {
// Bilinear blend of four corner samples; NaN propagates as "invalid".
inline float bilinearBlend(float p00, float p10, float p01, float p11,
                           float tx, float ty) {
    if (std::isnan(p00) || std::isnan(p10) || std::isnan(p01) || std::isnan(p11))
        return NAN;
    const float top = p00 + tx * (p10 - p00);
    const float bot = p01 + tx * (p11 - p01);
    return top + ty * (bot - top);
}
} // namespace

namespace {
inline bool isPlanar420(const Observation& o) {
    return o.avPixelFormat == AV_PIX_FMT_YUV420P
        || o.avPixelFormat == AV_PIX_FMT_YUVJ420P
        || o.avPixelFormat == AV_PIX_FMT_YUV420P10LE
        || o.avPixelFormat == AV_PIX_FMT_YUV420P12LE
        || o.avPixelFormat == AV_PIX_FMT_YUV420P16LE;
}
inline int bytesPerSample(const Observation& o) { return o.color.bitDepth > 8 ? 2 : 1; }
inline uint32_t maxSample(const Observation& o) {
    const int d=std::clamp(o.color.bitDepth,1,16);
    return d==16?65535u:((1u<<d)-1u);
}
float planeAt(const Observation& o,int plane,int x,int y,int pw,int ph){
    if(plane<0||plane>=o.planeCount||x<0||y<0||x>=pw||y>=ph)return NAN;
    const int bps=bytesPerSample(o);
    const size_t off=static_cast<size_t>(y)*o.linesize[plane]+static_cast<size_t>(x)*bps;
    if(off+static_cast<size_t>(bps)>o.plane[plane].size())return NAN;
    if(bps==1)return static_cast<float>(o.plane[plane][off]);
    return static_cast<float>(uint32_t(o.plane[plane][off])|(uint32_t(o.plane[plane][off+1])<<8));
}
void planeSet(Observation& o,int plane,int x,int y,float value,int pw,int ph){
    if(plane<0||plane>=o.planeCount||x<0||y<0||x>=pw||y>=ph)return;
    const int bps=bytesPerSample(o);
    const size_t off=static_cast<size_t>(y)*o.linesize[plane]+static_cast<size_t>(x)*bps;
    if(off+static_cast<size_t>(bps)>o.plane[plane].size())return;
    const uint32_t v=static_cast<uint32_t>(std::lround(
        std::clamp(value,0.0f,static_cast<float>(maxSample(o)))));
    o.plane[plane][off]=static_cast<uint8_t>(v&0xff);
    if(bps==2)o.plane[plane][off+1]=static_cast<uint8_t>((v>>8)&0xff);
}
inline float lumaAt(const Observation& o,int x,int y){
    return planeAt(o,0,x,y,o.width,o.height);
}

// Mean absolute difference of two blocks; large penalty when either block
// leaves the frame. Deterministic integer arithmetic.
uint32_t blockSad(const Observation& a, int ax, int ay, const Observation& b,
                  int bx, int by, int bw, int bh) {
    uint32_t sad = 0;
    for (int y = 0; y < bh; ++y) {
        const int ayi = ay + y, byi = by + y;
        if (ayi < 0 || ayi >= a.height || byi < 0 || byi >= b.height) return ~0u;
        for (int x = 0; x < bw; ++x) {
            const int axi = ax + x, bxi = bx + x;
            if (axi < 0 || axi >= a.width || bxi < 0 || bxi >= b.width) return ~0u;
            const int d=static_cast<int>(lumaAt(a,axi,ayi))
                       -static_cast<int>(lumaAt(b,bxi,byi));
            sad+=static_cast<uint32_t>(d<0?-d:d);
        }
    }
    return sad;
}
} // namespace

std::vector<BlockMotion> estimateCorrespondence(const Observation& target,
                                                const Observation& obs,
                                                int blockSize, int searchRadius) {
    std::vector<BlockMotion> out;
    if (target.plane[0].empty() || obs.plane[0].empty()) return out;
    const int bw = blockSize;
    // Tile the frame in blockSize steps with CLIPPED edge tiles: a 66x65
    // frame still yields 2-wide right-column tiles and 1-tall bottom-row
    // tiles, and 1920x1080 yields 8-tall bottom-row tiles. Skipping partial
    // tiles left edge pixels without correspondence, which previously
    // degenerated into fabricated zero-motion blending.
    for (int by = 0; by < target.height; by += bw) {
        const int bh = std::min(bw, target.height - by);
        for (int bx = 0; bx < target.width; bx += bw) {
            const int bwl = std::min(bw, target.width - bx);
            uint32_t bestSad = ~0u;
            int bestDx = 0, bestDy = 0;
            for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
                for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                    const uint32_t sad =
                        blockSad(target, bx, by, obs, bx + dx, by + dy, bwl, bh);
                    if (sad < bestSad) {
                        bestSad = sad;
                        bestDx = dx;
                        bestDy = dy;
                    }
                }
            }
            if (bestSad == ~0u) continue; // no in-bounds candidate for this tile
            BlockMotion m;
            m.frameIndex = target.frameIndex;
            m.refFrameIndex = static_cast<int64_t>(obs.frameIndex);
            m.temporalDistance = static_cast<int>(obs.frameIndex)
                               - static_cast<int>(target.frameIndex);
            m.direction = m.temporalDistance < 0 ? RefDirection::Past
                                                 : RefDirection::Future;
            m.dstX = static_cast<int16_t>(bx);
            m.dstY = static_cast<int16_t>(by);
            m.blockW = static_cast<uint16_t>(bwl);
            m.blockH = static_cast<uint16_t>(bh);
            m.mvX = static_cast<float>(bestDx);
            m.mvY = static_cast<float>(bestDy);
            m.precision = MotionPrecision::Integer;
            m.source = CorrespondenceSource::ImageEstimate;
            m.ambiguous = false;
            out.push_back(m);
        }
    }
    return out;
}

FlowField buildFlowField(int width, int height, const std::vector<BlockMotion>& blocks,
                         int64_t refFrameIndex, std::vector<uint8_t>* coverageOut) {
    FlowField flow(static_cast<size_t>(width) * height * 2, 0.0f);
    if (coverageOut) coverageOut->assign(static_cast<size_t>(width) * height, 0);
    for (const BlockMotion& b : blocks) {
        // Reference identity must be proven for the vector to enter
        // reconstruction. Ambiguous codec vectors are rejected here.
        if (b.ambiguous || b.refFrameIndex != refFrameIndex) continue;
        for (int y = b.dstY; y < b.dstY + b.blockH && y < height; ++y) {
            if (y < 0) continue;
            for (int x = b.dstX; x < b.dstX + b.blockW && x < width; ++x) {
                if (x < 0) continue;
                const size_t i = (static_cast<size_t>(y) * width + x) * 2;
                flow[i] = b.mvX;
                flow[i + 1] = b.mvY;
                if (coverageOut) (*coverageOut)[i / 2] = 1;
            }
        }
    }
    return flow;
}

namespace {
inline float bilinear(const Observation& o, float fx, float fy) {
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - x0, ty = fy - y0;
    const int x1=std::min(x0+1,o.width-1), y1=std::min(y0+1,o.height-1);
    auto inside=[&](int x,int y){return x>=0&&x<o.width&&y>=0&&y<o.height;};
    if(!inside(x0,y0))return NAN;
    auto at = [&](int x, int y) -> float {
        if (!inside(x, y)) return NAN;
        return lumaAt(o, x, y);
    };
    const float p00 = at(x0, y0), p10 = at(x1, y0), p01 = at(x0, y1), p11 = at(x1, y1);
    const float top = p00 + tx * (p10 - p00);
    const float bot = p01 + tx * (p11 - p01);
    return top + ty * (bot - top);
}
} // namespace

AccumulateResult accumulate(const Observation& target,
                            const std::vector<Observation>& neighbors,
                            const std::vector<FlowField>& neighborFlows,
                            const std::vector<std::vector<Visibility>>& neighborVisibility) {
    AccumulateResult res; res.frame=target;
    const int w=target.width,h=target.height;
    if(w<=0||h<=0||target.plane[0].empty())return res;

    std::vector<float> accY(static_cast<size_t>(w)*h,0.0f);
    std::vector<float> accW(static_cast<size_t>(w)*h,1.0f);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)
        accY[static_cast<size_t>(y)*w+x]=lumaAt(target,x,y);

    const bool p420=target.planeCount>2&&isPlanar420(target);
    const int cw=p420?(w+1)/2:0,ch=p420?(h+1)/2:0;
    std::vector<float> accU,accV,accCW;
    if(p420){
        accU.assign(static_cast<size_t>(cw)*ch,0.0f);
        accV.assign(static_cast<size_t>(cw)*ch,0.0f);
        accCW.assign(static_cast<size_t>(cw)*ch,1.0f);
        for(int y=0;y<ch;++y)for(int x=0;x<cw;++x){
            const size_t i=static_cast<size_t>(y)*cw+x;
            accU[i]=planeAt(target,1,x,y,cw,ch);
            accV[i]=planeAt(target,2,x,y,cw,ch);
        }
    }

    for(size_t n=0;n<neighbors.size()&&n<neighborFlows.size();++n){
        const Observation& o=neighbors[n];
        const FlowField& flow=neighborFlows[n];
        if(flow.size()<static_cast<size_t>(w)*h*2)continue;
        const std::vector<Visibility> none;
        const auto& vis=n<neighborVisibility.size()?neighborVisibility[n]:none;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){
            const size_t i=static_cast<size_t>(y)*w+x; ++res.totalSamples;
            if(!vis.empty()&&vis[i]==Visibility::Invalid)continue;
            const float s=bilinear(o,x+flow[i*2],y+flow[i*2+1]);
            if(std::isnan(s))continue;
            accY[i]+=s; accW[i]+=1.0f; ++res.validSamples;
        }
        if(p420&&isPlanar420(o)){
            for(int y=0;y<ch;++y)for(int x=0;x<cw;++x){
                const size_t i=static_cast<size_t>(y)*cw+x;
                const size_t fy=static_cast<size_t>(std::min(y*2,h-1))*w+std::min(x*2,w-1);
                if(!vis.empty()&&vis[fy]==Visibility::Invalid)continue;
                const float fx=static_cast<float>(x)+flow[fy*2]*0.5f;
                const float fyy=static_cast<float>(y)+flow[fy*2+1]*0.5f;
                const int x0=static_cast<int>(std::floor(fx)),y0=static_cast<int>(std::floor(fyy));
                if(x0<0||y0<0||x0>=cw||y0>=ch)continue;
                const int x1=std::min(x0+1,cw-1),y1=std::min(y0+1,ch-1);
                const float tx=fx-x0,ty=fyy-y0;
                const float u=bilinearBlend(planeAt(o,1,x0,y0,cw,ch),planeAt(o,1,x1,y0,cw,ch),
                    planeAt(o,1,x0,y1,cw,ch),planeAt(o,1,x1,y1,cw,ch),tx,ty);
                const float v=bilinearBlend(planeAt(o,2,x0,y0,cw,ch),planeAt(o,2,x1,y0,cw,ch),
                    planeAt(o,2,x0,y1,cw,ch),planeAt(o,2,x1,y1,cw,ch),tx,ty);
                if(std::isnan(u)||std::isnan(v))continue;
                accU[i]+=u;accV[i]+=v;accCW[i]+=1.0f;
            }
        }
    }
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        const size_t i=static_cast<size_t>(y)*w+x;
        planeSet(res.frame,0,x,y,accY[i]/accW[i],w,h);
    }
    if(p420)for(int y=0;y<ch;++y)for(int x=0;x<cw;++x){
        const size_t i=static_cast<size_t>(y)*cw+x;
        planeSet(res.frame,1,x,y,accU[i]/accCW[i],cw,ch);
        planeSet(res.frame,2,x,y,accV[i]/accCW[i],cw,ch);
    }
    return res;
}

} // namespace anvil
