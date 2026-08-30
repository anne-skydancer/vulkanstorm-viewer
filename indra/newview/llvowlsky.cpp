/**
 * @file llvowlsky.cpp
 * @brief LLVOWLSky class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2025, William Weaver (paperwork) @ Second Life
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "pipeline.h"

#include "llvowlsky.h"
#include "llsky.h"
#include "lldrawpoolwlsky.h"
#include "llface.h"
#include "llviewercontrol.h"
#include "llenvironment.h"
#include "llsettingssky.h"
#include "llviewercamera.h" // LLViewerCamera::getView()
#include "llviewerwindow.h" // gViewerWindow

constexpr U32 SKY_DETAIL = 18; // Any lower and there will be artifacts

// Classic (pre-procedural) star count, used when RenderStarfieldEnabled is false
constexpr U32 CLASSIC_STAR_COUNT = 1000;

inline U32 LLVOWLSky::getNumStacks(void)
{
    return SKY_DETAIL;
}

inline U32 LLVOWLSky::getNumSlices(void)
{
    return 2 * SKY_DETAIL;
}

inline U32 LLVOWLSky::getStripsNumVerts(void)
{
    return (getNumStacks() - 1) * getNumSlices();
}

inline U32 LLVOWLSky::getStripsNumIndices(void)
{
    return 2 * ((getNumStacks() - 2) * (getNumSlices() + 1)) + 1 ;
}

inline U32 LLVOWLSky::getStarsNumVerts(void)
{
    // The star vectors are sized by initStars() from the RenderStarfield*
    // settings (or the classic count), so the vector size is authoritative.
    return (U32)mStarVertices.size();
}

inline U32 LLVOWLSky::getStarsNumIndices(void)
{
    return 1000;
}

LLVOWLSky::LLVOWLSky(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
    : LLStaticViewerObject(id, pcode, regionp, true)
{
    initStars();
}

void LLVOWLSky::idleUpdate(LLAgent &agent, const F64 &time)
{

}

bool LLVOWLSky::isActive(void) const
{
    return false;
}

LLDrawable * LLVOWLSky::createDrawable(LLPipeline * pipeline)
{
    pipeline->allocDrawable(this);

    //LLDrawPoolWLSky *poolp = static_cast<LLDrawPoolWLSky *>(
        gPipeline.getPool(LLDrawPool::POOL_WL_SKY);

    mDrawable->setRenderType(LLPipeline::RENDER_TYPE_WL_SKY);

    return mDrawable;
}

// a tiny helper function for controlling the sky dome tesselation.
inline F32 calcPhi(const U32 &i, const F32 &reciprocal_num_stacks)
{
    // Calc: PI/8 * 1-((1-t^4)*(1-t^4))  { 0<t<1 }
    // Demos: \pi/8*\left(1-((1-x^{4})*(1-x^{4}))\right)\ \left\{0<x\le1\right\}

    // i should range from [0..SKY_STACKS] so t will range from [0.f .. 1.f]
    F32 t = float(i) * reciprocal_num_stacks; //SL-16127: remove: / float(getNumStacks());

    // ^4 the parameter of the tesselation to bias things toward 0 (the dome's apex)
    t *= t;
    t *= t;

    // invert and square the parameter of the tesselation to bias things toward 1 (the horizon)
    t = 1.f - t;
    t = t*t;
    t = 1.f - t;

    return (F_PI / 8.f) * t;
}

void LLVOWLSky::resetVertexBuffers()
{
    mStripsVerts.clear();
    mStarsVerts = nullptr;
    mFsSkyVerts = nullptr;

    gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
}

void LLVOWLSky::cleanupGL()
{
    mStripsVerts.clear();
    mStarsVerts = nullptr;
    mFsSkyVerts = nullptr;

    LLDrawPoolWLSky::cleanupGL();
}

void LLVOWLSky::restoreGL()
{
    LLDrawPoolWLSky::restoreGL();
    gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
}

bool LLVOWLSky::updateGeometry(LLDrawable * drawable)
{
    LL_PROFILE_ZONE_SCOPED;
    LLStrider<LLVector3>    vertices;
    LLStrider<LLVector2>    texCoords;
    LLStrider<U16>          indices;

    if (mFsSkyVerts.isNull())
    {
        mFsSkyVerts = new LLVertexBuffer(LLDrawPoolWLSky::ADV_ATMO_SKY_VERTEX_DATA_MASK);

        if (!mFsSkyVerts->allocateBuffer(4, 6))
        {
            LL_WARNS() << "Failed to allocate Vertex Buffer on full screen sky update" << LL_ENDL;
        }

        bool success = mFsSkyVerts->getVertexStrider(vertices)
                    && mFsSkyVerts->getTexCoord0Strider(texCoords)
                    && mFsSkyVerts->getIndexStrider(indices);

        if(!success)
        {
            LL_ERRS() << "Failed updating WindLight fullscreen sky geometry." << LL_ENDL;
        }

        *vertices++ = LLVector3(-1.0f, -1.0f, 0.0f);
        *vertices++ = LLVector3( 1.0f, -1.0f, 0.0f);
        *vertices++ = LLVector3(-1.0f,  1.0f, 0.0f);
        *vertices++ = LLVector3( 1.0f,  1.0f, 0.0f);

        *texCoords++ = LLVector2(0.0f, 0.0f);
        *texCoords++ = LLVector2(1.0f, 0.0f);
        *texCoords++ = LLVector2(0.0f, 1.0f);
        *texCoords++ = LLVector2(1.0f, 1.0f);

        *indices++ = 0;
        *indices++ = 1;
        *indices++ = 2;
        *indices++ = 1;
        *indices++ = 3;
        *indices++ = 2;

        mFsSkyVerts->unmapBuffer();
    }

    {
        const F32 dome_radius = LLEnvironment::instance().getCurrentSky()->getDomeRadius();

        const U32 max_buffer_bytes = gSavedSettings.getS32("RenderMaxVBOSize")*1024;
        const U32 data_mask = LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK;
        const U32 max_verts = max_buffer_bytes / LLVertexBuffer::calcVertexSize(data_mask);

        const U32 total_stacks = getNumStacks();

        const U32 verts_per_stack = getNumSlices();

        // each seg has to have one more row of verts than it has stacks
        // then round down
        const U32 stacks_per_seg = (max_verts - verts_per_stack) / verts_per_stack;

        // round up to a whole number of segments
        const U32 strips_segments = (total_stacks+stacks_per_seg-1) / stacks_per_seg;

        mStripsVerts.resize(strips_segments, NULL);

#if RELEASE_SHOW_DEBUG
        LL_INFOS() << "WL Skydome strips in " << strips_segments << " batches." << LL_ENDL;

        LLTimer timer;
        timer.start();
#endif

        for (U32 i = 0; i < strips_segments ;++i)
        {
            LLVertexBuffer * segment = new LLVertexBuffer(LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK);
            mStripsVerts[i] = segment;

            U32 num_stacks_this_seg = stacks_per_seg;
            if ((i == strips_segments - 1) && (total_stacks % stacks_per_seg) != 0)
            {
                // for the last buffer only allocate what we'll use
                num_stacks_this_seg = total_stacks % stacks_per_seg;
            }

            // figure out what range of the sky we're filling
            const U32 begin_stack = i * stacks_per_seg;
            const U32 end_stack = begin_stack + num_stacks_this_seg;
            llassert(end_stack <= total_stacks);

            const U32 num_verts_this_seg = verts_per_stack * (num_stacks_this_seg+1);
            llassert(num_verts_this_seg <= max_verts);

            const U32 num_indices_this_seg = 1+num_stacks_this_seg*(2+2*verts_per_stack);
            llassert(num_indices_this_seg * sizeof(U16) <= max_buffer_bytes);

            bool allocated = segment->allocateBuffer(num_verts_this_seg, num_indices_this_seg);
#if RELEASE_SHOW_WARNS
            if( !allocated )
            {
                LL_WARNS() << "Failed to allocate Vertex Buffer on update to "
                    << num_verts_this_seg << " vertices and "
                    << num_indices_this_seg << " indices" << LL_ENDL;
            }
#else
            (void) allocated;
#endif

            // lock the buffer
            bool success = segment->getVertexStrider(vertices)
                && segment->getTexCoord0Strider(texCoords)
                && segment->getIndexStrider(indices);

#if RELEASE_SHOW_DEBUG
            if(!success)
            {
                LL_ERRS() << "Failed updating WindLight sky geometry." << LL_ENDL;
            }
#else
            (void) success;
#endif

            // fill it
            buildStripsBuffer(begin_stack, end_stack, vertices, texCoords, indices, dome_radius, verts_per_stack, total_stacks);

            // and unlock the buffer
            segment->unmapBuffer();
        }

#if RELEASE_SHOW_DEBUG
        LL_INFOS() << "completed in " << llformat("%.2f", timer.getElapsedTimeF32().value()) << "seconds" << LL_ENDL;
#endif
    }

    updateStarColors();
    updateStarGeometry(drawable);

    LLPipeline::sCompiles++;

    return true;
}

void LLVOWLSky::drawStars(void)
{
    //  render the stars as a sphere centered at viewer camera
    if (mStarsVerts.notNull())
    {
        mStarsVerts->setBuffer();
        // 6 vertices per star quad: match the geometry built in updateStarGeometry
        mStarsVerts->drawArrays(LLRender::TRIANGLES, 0, getStarsNumVerts()*6);
    }
}

void LLVOWLSky::drawFsSky(void)
{
    if (mFsSkyVerts.isNull())
    {
        updateGeometry(mDrawable);
    }

    LLGLDisable disable_blend(GL_BLEND);

    mFsSkyVerts->setBuffer();
    mFsSkyVerts->drawRange(LLRender::TRIANGLES, 0, mFsSkyVerts->getNumVerts() - 1, mFsSkyVerts->getNumIndices(), 0);
    gPipeline.addTrianglesDrawn(mFsSkyVerts->getNumIndices());
    LLVertexBuffer::unbind();
}

void LLVOWLSky::drawDome(void)
{
    if (mStripsVerts.empty())
    {
        updateGeometry(mDrawable);
    }

    LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

    std::vector< LLPointer<LLVertexBuffer> >::const_iterator strips_vbo_iter, end_strips;
    end_strips = mStripsVerts.end();
    for(strips_vbo_iter = mStripsVerts.begin(); strips_vbo_iter != end_strips; ++strips_vbo_iter)
    {
        LLVertexBuffer * strips_segment = strips_vbo_iter->get();

        strips_segment->setBuffer();

        strips_segment->drawRange(
            LLRender::TRIANGLE_STRIP,
            0, strips_segment->getNumVerts()-1, strips_segment->getNumIndices(),
            0);
        gPipeline.addTrianglesDrawn(strips_segment->getNumIndices());
    }

    LLVertexBuffer::unbind();
}

LLColor4 LLVOWLSky::blackBodyColor(F32 temperature)
{
    LLColor4 color;
    temperature /= 100.0f;

    // Red
    if (temperature <= 66.0f) {
        color.mV[VRED] = 1.0f;
    } else {
        color.mV[VRED] = temperature - 60.0f;
        color.mV[VRED] = 329.698727446f * pow(color.mV[VRED], -0.1332047592f);
        color.mV[VRED] = llclamp(color.mV[VRED] / 255.0f, 0.0f, 1.0f);
    }

    // Green
    if (temperature <= 66.0f) {
        color.mV[VGREEN] = temperature;
        color.mV[VGREEN] = 99.4708025861f * log(color.mV[VGREEN]) - 161.1195681661f;
        color.mV[VGREEN] = llclamp(color.mV[VGREEN] / 255.0f, 0.0f, 1.0f);
    } else {
        color.mV[VGREEN] = temperature - 60.0f;
        color.mV[VGREEN] = 288.1221695283f * pow(color.mV[VGREEN], -0.0755148492f);
        color.mV[VGREEN] = llclamp(color.mV[VGREEN] / 255.0f, 0.0f, 1.0f);
    }

    // Blue
    if (temperature >= 66.0f) {
        color.mV[VBLUE] = 1.0f;
    } else if (temperature <= 19.0f) {
        color.mV[VBLUE] = 0.0f;
    } else {
        color.mV[VBLUE] = temperature - 10;
        color.mV[VBLUE] = 138.5177312231f * log(color.mV[VBLUE]) - 305.0447927307f;
        color.mV[VBLUE] = llclamp(color.mV[VBLUE] / 255.0f, 0.0f, 1.0f);
    }

    color.mV[VALPHA] = 1.0f;
    return color;
}

// The main initStars function orchestrates the generation
void LLVOWLSky::initStars()
{
    if (!gSavedSettings.getBOOL("RenderStarfieldEnabled"))
    {
        initStarsClassic();
        return;
    }

    const U32 num_primary = gSavedSettings.getU32("RenderStarfieldPrimaryCount");
    const U32 num_dust = gSavedSettings.getU32("RenderStarfieldDustCount");

    // Resize vectors to hold ALL stars (primary + dust)
    mStarVertices.resize(num_primary + num_dust);
    mStarColors.resize(num_primary + num_dust);
    mStarIntensities.resize(num_primary + num_dust);

    mProceduralStarfield = true;

    // --- Phase 1: Generate Primary Stars ---
    // Use parameters similar to the original code for brighter stars
    const F32 primary_min_intensity = 0.05f;
    const F32 primary_max_intensity = 1.0f;
    const F32 primary_brightness_exponent = 3.5f;
    const F32 primary_color_variation = 0.30f;

    generateProceduralStars(
        num_primary, 0,
        primary_min_intensity, primary_max_intensity, primary_brightness_exponent, primary_color_variation,
        mStarVertices, mStarColors, mStarIntensities
    );

    // --- Phase 2: Generate Dust Stars ---
    // Use parameters for much fainter stars
    const F32 dust_min_intensity = 0.01f;       // Lower minimum intensity
    const F32 dust_max_intensity = 0.15f;       // SIGNIFICANTLY lower maximum intensity
    const F32 dust_brightness_exponent = 5.0f;  // Higher exponent skews towards faintness
    const F32 dust_color_variation = 0.15f;     // Less color variation for faint dust

    generateProceduralStars(
        num_dust, num_primary,
        dust_min_intensity, dust_max_intensity, dust_brightness_exponent, dust_color_variation,
        mStarVertices, mStarColors, mStarIntensities
    );
}

void LLVOWLSky::generateProceduralStars(
    U32 count, U32 startIndex,
    F32 min_intensity, F32 max_intensity, F32 brightness_exponent, F32 color_variation,
    std::vector<LLVector3>& vertices, std::vector<LLColor4>& colors, std::vector<F32>& intensities)
{
    const F32 DISTANCE_TO_STARS = LLEnvironment::instance().getCurrentSky()->getDomeRadius();

    // Milky Way settings
    const bool enable_milky_way = gSavedSettings.getBOOL("RenderStarfieldMilkyWay");
    const F32 milky_way_density_factor = 15.0f;
    LLVector3 milky_way_normal(0.5f, 0.0f, 0.866f);
    milky_way_normal.normVec();
    const F32 milky_way_thickness_factor = 0.4f;

    // Iterators to the correct starting position in the vectors
    std::vector<LLVector3>::iterator v_p = vertices.begin() + startIndex;
    std::vector<LLColor4>::iterator v_c = colors.begin() + startIndex;
    std::vector<F32>::iterator v_i = intensities.begin() + startIndex;

    // Temperature range for black body color calculation (Kelvin)
    const F32 min_temperature = 3000.0f;  // Cool Red M-type approx
    const F32 max_temperature = 25000.0f; // Hot Blue/White B/O-type approx
    const F32 temperature_range = max_temperature - min_temperature;

    U32 stars_generated = 0;
    while (stars_generated < count)
    {
        // 1. Generate a candidate position randomly on a full sphere
        LLVector3 candidate_pos;
        candidate_pos.mV[VX] = ll_frand() * 2.0f - 1.0f;
        candidate_pos.mV[VY] = ll_frand() * 2.0f - 1.0f;
        candidate_pos.mV[VZ] = ll_frand() * 2.0f - 1.0f;

        if (candidate_pos.magVec() < 1e-6f) { continue; }
        candidate_pos.normVec();

        // 2. Determine probability based on Milky Way simulation
        F32 acceptance_probability = 1.0f;
        if (enable_milky_way)
        {
            F32 dist_from_plane = fabsf(candidate_pos * milky_way_normal);
            acceptance_probability *= pow(1.0f - dist_from_plane, 1.0f / milky_way_thickness_factor) * (milky_way_density_factor - 1.0f) + 1.0f;
        }

        // 3. Probabilistically decide whether to keep this star
        if (ll_frand() * acceptance_probability < 1.0f) { continue; }

        // --- Accept the star ---

        // 4. Assign final position (using distance tiers)
        F32 distance_scale = DISTANCE_TO_STARS;
        F32 distance_tier2 = DISTANCE_TO_STARS * 100.0f;
        F32 distance_tier3 = DISTANCE_TO_STARS * 1000.0f;
        F32 prob = ll_frand();
        if (prob < 0.15) { distance_scale = DISTANCE_TO_STARS; }
        else if (prob > .50) { distance_scale = distance_tier3; }
        else { distance_scale = distance_tier2; }
        *v_p = candidate_pos * distance_scale;

        // 5. Calculate intrinsic intensity (BEFORE distance dimming)
        F32 intensity = pow(ll_frand(), brightness_exponent + (ll_frand() - 0.5f) * 1.0f);
        intensity = min_intensity + intensity * (max_intensity - min_intensity);
        intensity = llclamp(intensity, min_intensity, max_intensity); // Clamp to the layer's max BEFORE dimming

        // Store the intensity *before* distance dimming for color calculation
        F32 intensity_for_color = intensity;

        // Apply dimming based on distance to get the final intensity for brightness/size
        F32 distance_dim_factor = 1.0f;
        if (distance_scale == distance_tier2) { distance_dim_factor = 0.50f; }
        else if (distance_scale == distance_tier3) { distance_dim_factor = 0.10f; }
        intensity *= distance_dim_factor;
        intensity = llmax(intensity, 0.0f);
        *v_i = intensity; // Final intensity drives brightness/size/twinkle

        // 6. Calculate color (using pre-dimming intensity)
        F32 intensity_factor_for_color = 0.0f;
        float layer_intensity_range = max_intensity - min_intensity;
        if (layer_intensity_range > 1e-5f) // Avoid divide by zero
        {
            // Factor based on where the PRE-DIMMING intensity falls in the layer's range
            intensity_factor_for_color = (intensity_for_color - min_intensity) / layer_intensity_range;
            intensity_factor_for_color = llclamp(intensity_factor_for_color, 0.0f, 1.0f);
        }

        // Non-linear mapping for better visual spread: exponent > 1 pushes
        // values towards min_temperature (more reddish stars)
        float color_curve_exponent = 3.0f;
        float curved_intensity_factor = pow(intensity_factor_for_color, color_curve_exponent);

        F32 temperature = min_temperature + curved_intensity_factor * temperature_range;
        temperature = llclamp(temperature, min_temperature, max_temperature);

        LLColor4 star_color = blackBodyColor(temperature);

        // Add random color variation
        star_color.mV[VRED]   += (ll_frand() * 2.0f - 1.0f) * color_variation;
        star_color.mV[VGREEN] += (ll_frand() * 2.0f - 1.0f) * color_variation;
        star_color.mV[VBLUE]  += (ll_frand() * 2.0f - 1.0f) * color_variation;
        star_color.mV[VALPHA] = 1.0f;
        star_color.clamp();
        *v_c = star_color;

        // Increment iterators and count
        v_p++;
        v_c++;
        v_i++;
        stars_generated++;
    }
}

void LLVOWLSky::initStarsClassic()
{
    const F32 DISTANCE_TO_STARS = LLEnvironment::instance().getCurrentSky()->getDomeRadius();

    // Initialize star map
    mStarVertices.resize(CLASSIC_STAR_COUNT);
    mStarColors.resize(CLASSIC_STAR_COUNT);
    mStarIntensities.resize(CLASSIC_STAR_COUNT);

    std::vector<LLVector3>::iterator v_p = mStarVertices.begin();
    std::vector<LLColor4>::iterator v_c = mStarColors.begin();
    std::vector<F32>::iterator v_i = mStarIntensities.begin();

    U32 i;

    for (i = 0; i < CLASSIC_STAR_COUNT; ++i)
    {
        v_p->mV[VX] = ll_frand() - 0.5f;
        v_p->mV[VY] = ll_frand() - 0.5f;

        // we only want stars on the top half of the dome!

        v_p->mV[VZ] = ll_frand()/2.f;

        v_p->normVec();
        *v_p *= DISTANCE_TO_STARS;
        *v_i = llmin((F32)pow(ll_frand(),2.f) + 0.1f, 1.f);
        v_c->mV[VRED]   = 0.75f + ll_frand() * 0.25f ;
        v_c->mV[VGREEN] = 1.f ;
        v_c->mV[VBLUE]  = 0.75f + ll_frand() * 0.25f ;
        v_c->mV[VALPHA] = 1.f;
        v_c->clamp();
        v_p++;
        v_c++;
        v_i++;
    }
}

void LLVOWLSky::buildStripsBuffer(U32 begin_stack,
                                  U32 end_stack,
                                  LLStrider<LLVector3> & vertices,
                                  LLStrider<LLVector2> & texCoords,
                                  LLStrider<U16> & indices,
                                  const F32 dome_radius,
                                  const U32& num_slices,
                                  const U32& num_stacks)
{
    U32 i, j;
    F32 phi0, theta, x0, y0, z0;
    const F32 reciprocal_num_stacks = 1.f / num_stacks;

    llassert(end_stack <= num_stacks);

    // stacks are iterated one-indexed since phi(0) was handled by the fan above
#if NEW_TESS
    for(i = begin_stack; i <= end_stack; ++i)
#else
    for(i = begin_stack + 1; i <= end_stack+1; ++i)
#endif
    {
        phi0 = calcPhi(i, reciprocal_num_stacks);

        for(j = 0; j < num_slices; ++j)
        {
            theta = F_TWO_PI * (float(j) / float(num_slices));

            // standard transformation from  spherical to
            // rectangular coordinates
            x0 = sin(phi0) * cos(theta);
            y0 = cos(phi0);
            z0 = sin(phi0) * sin(theta);

#if NEW_TESS
            *vertices++ = LLVector3(x0 * dome_radius, y0 * dome_radius, z0 * dome_radius);
#else
            if (i == num_stacks-2)
            {
                *vertices++ = LLVector3(x0*dome_radius, y0*dome_radius-1024.f*2.f, z0*dome_radius);
            }
            else if (i == num_stacks-1)
            {
                *vertices++ = LLVector3(0, y0*dome_radius-1024.f*2.f, 0);
            }
            else
            {
                *vertices++     = LLVector3(x0 * dome_radius, y0 * dome_radius, z0 * dome_radius);
            }
#endif

            // generate planar uv coordinates
            // note: x and z are transposed in order for things to animate
            // correctly in the global coordinate system where +x is east and
            // +y is north
            *texCoords++    = LLVector2((-z0 + 1.f) / 2.f, (-x0 + 1.f) / 2.f);
        }
    }

    //build triangle strip...
    *indices++ = 0 ;

    S32 k = 0 ;
    for(i = 1; i <= end_stack - begin_stack; ++i)
    {
        *indices++ = i * num_slices + k ;

        k = (k+1) % num_slices ;
        for(j = 0; j < num_slices ; ++j)
        {
            *indices++ = (i-1) * num_slices + k ;
            *indices++ = i * num_slices + k ;

            k = (k+1) % num_slices ;
        }

        if((--k) < 0)
        {
            k = num_slices - 1 ;
        }

        *indices++ = i * num_slices + k ;
    }
}

void LLVOWLSky::updateStarColors()
{
    std::vector<LLColor4>::iterator v_c = mStarColors.begin();
    std::vector<F32>::iterator v_i = mStarIntensities.begin();
    std::vector<LLVector3>::iterator v_p = mStarVertices.begin();

    const F32 var = 0.15f;
    const F32 min = 0.5f; //0.75f;
    //const F32 sunclose_max = 0.6f;
    //const F32 sunclose_range = 1 - sunclose_max;

    //F32 below_horizon = - llmin(0.0f, gSky.mVOSkyp->getToSunLast().mV[2]);
    //F32 brightness_factor = llmin(1.0f, below_horizon * 20);

    static S32 swap = 0;
    swap++;

    if ((swap % 2) == 1)
    {
        F32 intensity;                      //  max intensity of each star
        U32 x;
        for (x = 0; x < getStarsNumVerts(); ++x)
        {
            //F32 sundir_factor = 1;
            LLVector3 tostar = *v_p;
            tostar.normVec();
            //const F32 how_close_to_sun = tostar * gSky.mVOSkyp->getToSunLast();
            //if (how_close_to_sun > sunclose_max)
            //{
            //  sundir_factor = (1 - how_close_to_sun) / sunclose_range;
            //}
            intensity = *(v_i);
            F32 alpha = v_c->mV[VALPHA] + (ll_frand() - 0.5f) * var * intensity;
            if (alpha < min * intensity)
            {
                alpha = min * intensity;
            }
            if (alpha > intensity)
            {
                alpha = intensity;
            }
            //alpha *= brightness_factor * sundir_factor;

            alpha = llclamp(alpha, 0.f, 1.f);
            v_c->mV[VALPHA] = alpha;
            v_c++;
            v_i++;
            v_p++;
        }
    }
}

bool LLVOWLSky::updateStarGeometry(LLDrawable *drawable)
{
    LLStrider<LLVector3> verticesp;
    LLStrider<LLColor4U> colorsp;
    LLStrider<LLVector2> texcoordsp;
    LLStrider<F32> intensityp;

    if (mStarsVerts.isNull())
    {
        mStarsVerts = new LLVertexBuffer(LLDrawPoolWLSky::STAR_VERTEX_DATA_MASK);
        if (!mStarsVerts->allocateBuffer(getStarsNumVerts()*6, 0))
        {
            LL_WARNS() << "Failed to allocate Vertex Buffer for Sky to " << getStarsNumVerts() * 6 << " vertices" << LL_ENDL;
            return false;
        }
    }

    bool success = mStarsVerts->getVertexStrider(verticesp)
        && mStarsVerts->getColorStrider(colorsp)
        && mStarsVerts->getTexCoord0Strider(texcoordsp)
        && mStarsVerts->getWeightStrider(intensityp);

    if(!success)
    {
        LL_WARNS() << "Failed updating star geometry." << LL_ENDL;
        mStarsVerts->unmapBuffer();
        return false;
    }

    if (mStarVertices.size() < getStarsNumVerts() || mStarIntensities.size() < getStarsNumVerts())
    {
        LL_WARNS() << "Star reference geometry insufficient." << LL_ENDL;
        mStarsVerts->unmapBuffer();
        return false;
    }

    // Resolution-dependent scaling context for the procedural starfield
    const F32 DISTANCE_TO_STARS = LLEnvironment::instance().getCurrentSky()->getDomeRadius();
    const F32 distance_tier2 = DISTANCE_TO_STARS * 100.0f;
    const F32 distance_tier3 = DISTANCE_TO_STARS * 1000.0f;
    const F32 fov_radians = LLViewerCamera::instance().getView();
    const F32 screen_height = (F32)gViewerWindow->getWindowHeightRaw();

    for (U32 vtx = 0; vtx < getStarsNumVerts(); ++vtx)
    {
        LLVector3 at = mStarVertices[vtx];
        at.normVec();
        LLVector3 left = at%LLVector3(0,0,1);
        LLVector3 up = at%left;

        F32 sc;
        if (!mProceduralStarfield)
        {
            // Classic sizing (exact legacy behavior)
            sc = 16.0f + (ll_frand() * 20.0f);
        }
        else
        {
            // Determine this star's distance tier from its magnitude
            const F32 mag = mStarVertices[vtx].magVec();
            F32 distance_scale = DISTANCE_TO_STARS;
            if (mag > (distance_tier3 - 0.1f))
            {
                distance_scale = distance_tier3;
            }
            else if (mag > (distance_tier2 - 0.1f))
            {
                distance_scale = distance_tier2;
            }

            // World size that projects to ~1 pixel at this star's distance
            const F32 base = distance_scale / (screen_height / (2.0f * tanf(fov_radians / 2.0f)));

            // Max apparent pixel size ratio for the brightest stars, by resolution category
            F32 target_max_pixel_ratio;
            if (screen_height <= 720.0f)
            {
                target_max_pixel_ratio = 1.33f;
            }
            else if (screen_height <= 1080.0f)
            {
                target_max_pixel_ratio = 2.0f;
            }
            else if (screen_height <= 2160.0f)
            {
                target_max_pixel_ratio = 4.0f;
            }
            else
            {
                target_max_pixel_ratio = 8.0f;
            }

            const F32 min_world_size = base;
            F32 world_size_influence = base * (target_max_pixel_ratio - 1.0f);
            if (world_size_influence < 0.0f)
            {
                world_size_influence = 0.0f;
            }

            // Scale with this star's intensity
            sc = min_world_size + mStarIntensities[vtx] * world_size_influence;
        }

        left *= sc;
        up *= sc;

        *(verticesp++)  = mStarVertices[vtx];
        *(verticesp++) = mStarVertices[vtx]+up;
        *(verticesp++) = mStarVertices[vtx]+left+up;
        *(verticesp++)  = mStarVertices[vtx];
        *(verticesp++) = mStarVertices[vtx]+left+up;
        *(verticesp++) = mStarVertices[vtx]+left;

        *(texcoordsp++) = LLVector2(1,0);
        *(texcoordsp++) = LLVector2(1,1);
        *(texcoordsp++) = LLVector2(0,1);
        *(texcoordsp++) = LLVector2(1,0);
        *(texcoordsp++) = LLVector2(0,1);
        *(texcoordsp++) = LLVector2(0,0);

        const LLColor4U color4u(mStarColors[vtx]);
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;

        // Per-star intensity stream (weight attribute)
        const F32 current_intensity = mStarIntensities[vtx];
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
    }

    mStarsVerts->unmapBuffer();
    return true;
}
