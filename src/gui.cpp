// Copyright (C) 2020 averne
//
// This file is part of Turnips.
//
// Turnips is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Turnips is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Turnips.  If not, see <http://www.gnu.org/licenses/>.
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <switch.h>
#include <imgui.h>
#include <stb_image.h>
#include "imgui_nx/imgui_deko3d.h"
#include "imgui_nx/imgui_nx.h"
#include "gui.hpp"
#include "theme.hpp"

namespace gui {

namespace {

constexpr std::uint32_t BG_IMAGE_ID   = 1;
constexpr std::uint32_t BG_SAMPLER_ID = 1;

constexpr auto MAX_SAMPLERS = 2;
constexpr auto MAX_IMAGES   = 2;

constexpr auto FB_NUM       = 2u;

constexpr auto CMDBUF_SIZE  = 1024 * 1024;

unsigned s_width  = 1920;
unsigned s_height = 1080;

dk::UniqueDevice       s_device;

dk::UniqueMemBlock     s_depthMemBlock;
dk::Image              s_depthBuffer;

dk::UniqueMemBlock     s_fbMemBlock;
dk::Image              s_frameBuffers[FB_NUM];

dk::UniqueMemBlock     s_cmdMemBlock[FB_NUM];
dk::UniqueCmdBuf       s_cmdBuf[FB_NUM];

dk::UniqueMemBlock     s_imageMemBlock;

dk::UniqueMemBlock     s_descriptorMemBlock;
dk::SamplerDescriptor *s_samplerDescriptors = nullptr;
dk::ImageDescriptor   *s_imageDescriptors   = nullptr;

dk::UniqueQueue        s_queue;
dk::UniqueSwapchain    s_swapchain;

//主功能 变量
    enum  {Backup,Delete,verify,Import};
    int e = 0 ;
    bool is_button = false ;
    bool result ;
    bool backup_flag = false;
    bool delete_flag = false;
    bool verify_flag = false;
    bool import_flag = false;

    Incognito incognito;
 //语言设置变量
int e_setting_language = 0;

void draw_setting()
{
    ImGui::RadioButton("中文", &e_setting_language, Language::Chinese);
    ImGui::RadioButton("English", &e_setting_language, Language::English);
    switch(e_setting_language)
    {
            case Language::Chinese:
                    Lang::setLanguage(Chinese);
                    break;
            case Language::English:
                    Lang::setLanguage(English);
                    break;
            default:
                    break;
    }    
}

void rebuildSwapchain(unsigned const width_, unsigned const height_) {
    // destroy old swapchain
    s_swapchain = nullptr;

    // create new depth buffer image layout
    dk::ImageLayout depthLayout;
    dk::ImageLayoutMaker{s_device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_Z24S8)
        .setDimensions(width_, height_)
        .initialize(depthLayout);

    auto const depthAlign = depthLayout.getAlignment();
    auto const depthSize  = depthLayout.getSize();

    // create depth buffer memblock
    if (!s_depthMemBlock) {
        s_depthMemBlock = dk::MemBlockMaker{s_device,
            imgui::deko3d::align(
                depthSize, std::max<unsigned> (depthAlign, DK_MEMBLOCK_ALIGNMENT))}
                              .setFlags(DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
                              .create();
    }

    s_depthBuffer.initialize(depthLayout, s_depthMemBlock, 0);

    // create framebuffer image layout
    dk::ImageLayout fbLayout;
    dk::ImageLayoutMaker{s_device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(width_, height_)
        .initialize(fbLayout);

    auto const fbAlign = fbLayout.getAlignment();
    auto const fbSize  = fbLayout.getSize();

    // create framebuffer memblock
    if (!s_fbMemBlock) {
        s_fbMemBlock = dk::MemBlockMaker{s_device,
            imgui::deko3d::align(
                FB_NUM * fbSize, std::max<unsigned> (fbAlign, DK_MEMBLOCK_ALIGNMENT))}
                           .setFlags(DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
                           .create();
    }

    // initialize swapchain images
    std::array<DkImage const *, FB_NUM> swapchainImages;
    for (unsigned i = 0; i < FB_NUM; ++i) {
        swapchainImages[i] = &s_frameBuffers[i];
        s_frameBuffers[i].initialize(fbLayout, s_fbMemBlock, i * fbSize);
    }

    // create swapchain
    s_swapchain = dk::SwapchainMaker{s_device, nwindowGetDefault(), swapchainImages}.create();
}

void deko3dInit() {
    // create deko3d device
    s_device = dk::DeviceMaker{}.create();

    // initialize swapchain with maximum resolution
    rebuildSwapchain(1920, 1080);

    // create memblocks for each image slot
    for (std::size_t i = 0; i < FB_NUM; ++i) {
        // create command buffer memblock
        s_cmdMemBlock[i] =
            dk::MemBlockMaker{s_device, imgui::deko3d::align(CMDBUF_SIZE, DK_MEMBLOCK_ALIGNMENT)}
                .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
                .create();

        // create command buffer
        s_cmdBuf[i] = dk::CmdBufMaker{s_device}.create();
        s_cmdBuf[i].addMemory(s_cmdMemBlock[i], 0, s_cmdMemBlock[i].getSize());
    }

    // create image/sampler memblock
    static_assert(sizeof(dk::ImageDescriptor)   == DK_IMAGE_DESCRIPTOR_ALIGNMENT);
    static_assert(sizeof(dk::SamplerDescriptor) == DK_SAMPLER_DESCRIPTOR_ALIGNMENT);
    static_assert(DK_IMAGE_DESCRIPTOR_ALIGNMENT == DK_SAMPLER_DESCRIPTOR_ALIGNMENT);
    s_descriptorMemBlock = dk::MemBlockMaker{s_device,
        imgui::deko3d::align(
            (MAX_SAMPLERS + MAX_IMAGES) * sizeof(dk::ImageDescriptor), DK_MEMBLOCK_ALIGNMENT)}
                               .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
                               .create();

    // get cpu address for descriptors
    s_samplerDescriptors =
        static_cast<dk::SamplerDescriptor *> (s_descriptorMemBlock.getCpuAddr());
    s_imageDescriptors =
        reinterpret_cast<dk::ImageDescriptor *> (&s_samplerDescriptors[MAX_SAMPLERS]);

    // create queue
    s_queue = dk::QueueMaker{s_device}.setFlags(DkQueueFlags_Graphics).create();

    auto &cmdBuf = s_cmdBuf[0];

    // bind image/sampler descriptors
    cmdBuf.bindSamplerDescriptorSet(s_descriptorMemBlock.getGpuAddr(), MAX_SAMPLERS);
    cmdBuf.bindImageDescriptorSet(
        s_descriptorMemBlock.getGpuAddr() + MAX_SAMPLERS * sizeof(dk::SamplerDescriptor),
        MAX_IMAGES);
    s_queue.submitCommands(cmdBuf.finishList());
    s_queue.waitIdle();

    cmdBuf.clear();
}

void deko3dExit() {
    // clean up all of the deko3d objects
    s_imageMemBlock      = nullptr;
    s_descriptorMemBlock = nullptr;

    for (unsigned i = 0; i < FB_NUM; ++i) {
        s_cmdBuf[i]      = nullptr;
        s_cmdMemBlock[i] = nullptr;
    }

    s_queue         = nullptr;
    s_swapchain     = nullptr;
    s_fbMemBlock    = nullptr;
    s_depthMemBlock = nullptr;
    s_device        = nullptr;
}

} // namespace

bool init() {
    ImGui::CreateContext();
    if (!imgui::nx::init())
        return false;

    deko3dInit();
    imgui::deko3d::init(s_device,
        s_queue,
        s_cmdBuf[0],
        s_samplerDescriptors[0],
        s_imageDescriptors[0],
        dkMakeTextureHandle(0, 0),
        FB_NUM);

    return true;
}

bool loop() {
    if (!appletMainLoop())
        return false;

    hidScanInput();

    auto const keysDown = hidKeysDown(CONTROLLER_P1_AUTO);

    // check if the user wants to exit
    if (keysDown & KEY_PLUS)
        return false;
    ///设置显示新界面
    imgui::nx::newFrame();  //自定义gui绘制
    
    ImGui::NewFrame();      //官方gui绘制




    // Add background image
    ImGui::GetBackgroundDrawList()->AddImage(
        imgui::deko3d::makeTextureID(dkMakeTextureHandle(BG_IMAGE_ID, BG_SAMPLER_ID)),
        ImVec2(0, 0),
        ImGui::GetIO().DisplaySize);

    return true;
}
void draw_main_function(){

    


    ImGui::RadioButton(Lang::stringtochar("button_backup"_lang), &e, 0); 
    ImGui::RadioButton(Lang::stringtochar("button_delete"_lang), &e, 1); 
    ImGui::RadioButton(Lang::stringtochar("button_verfiy"_lang), &e, 2);
    ImGui::RadioButton(Lang::stringtochar("button_import"_lang), &e, 3);
    
    is_button = ImGui::Button(Lang::stringtochar("select_order"_lang));

    switch (e)
    {
        case Backup:
            
            im::Text(Lang::stringtochar("backup_make_sure"_lang));
            if(is_button)
            {
                result =  incognito.backup();
                backup_flag = true;
                delete_flag = false;
                verify_flag = false;
                import_flag = false;
            }
            if(backup_flag)
            {
                if(result)
                    im::Text(Lang::stringtochar("backup_success"_lang)  );
                else 
                    im::Text(Lang::stringtochar("backup_fail"_lang));
            }
            break;
            
        case Delete:
            
            im::Text(Lang::stringtochar("delete_make_sure"_lang));
            if(is_button)
            {
                result = incognito.clean();
                backup_flag = false;
                delete_flag = true;
                verify_flag = false;
                import_flag = false;
            }
            if(delete_flag)
            {
                if(result)
                    im::Text(Lang::stringtochar("delete_success"_lang));
                else 
                    im::Text(Lang::stringtochar("delete_fail"_lang));
            }
            break;
            
        case verify:
            
            im::Text(Lang::stringtochar("verify_make_sure"_lang));
            if(is_button)
            {
                result = incognito.verify();
                backup_flag = false;
                delete_flag = false;
                verify_flag = true;
                import_flag = false;
            }
            if(verify_flag)
            {
                if (result)
                    im::Text(Lang::stringtochar("verify_success"_lang),incognito.serial());
                else
                    im::Text(Lang::stringtochar("verify_fail"_lang));
            }
            break;
            
        case Import:
            
            im::Text(Lang::stringtochar("import_make_sure"_lang));
            if(is_button)
            {
                result = incognito.import("sdmc:/backup/prodinfo.bin");
                backup_flag = false;
                delete_flag = false;
                verify_flag = false;
                import_flag = true;
            }
            if(import_flag)
            {
                if (result)
                    im::Text(Lang::stringtochar("import_success"_lang),incognito.serial());
                else 
                    im::Text(Lang::stringtochar("import_fail"_lang));
            }
            break;
        default:
            break;
    }

}

void render() {
    ImGui::Render();

    auto &io = ImGui::GetIO();

    if (s_width != io.DisplaySize.x || s_height != io.DisplaySize.y) {
        s_width  = io.DisplaySize.x;
        s_height = io.DisplaySize.y;
        rebuildSwapchain(s_width, s_height);
    }

    // get image from queue
    auto const slot = s_queue.acquireImage(s_swapchain);
    auto &cmdBuf    = s_cmdBuf[slot];

    cmdBuf.clear();

    // bind frame/depth buffers and clear them
    dk::ImageView colorTarget{s_frameBuffers[slot]};
    dk::ImageView depthTarget{s_depthBuffer};
    cmdBuf.bindRenderTargets(&colorTarget, &depthTarget);
    cmdBuf.setScissors(0, DkScissor{0, 0, s_width, s_height});
    cmdBuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    cmdBuf.clearDepthStencil(true, 1.0f, 0xFF, 0);
    s_queue.submitCommands(cmdBuf.finishList());

    imgui::deko3d::render(s_device, s_queue, cmdBuf, slot);

    // wait for fragments to be completed before discarding depth/stencil buffer
    cmdBuf.barrier(DkBarrier_Fragments, 0);
    cmdBuf.discardDepthStencil();

    // present image
    s_queue.presentImage(s_swapchain, slot);
}

void exit() {
    imgui::nx::exit();

    // wait for queue to be idle
    s_queue.waitIdle();

    imgui::deko3d::exit();
    deko3dExit();
}

bool create_background(const std::string &path) {
    int w, h, nchan;
    auto *data = stbi_load(path.c_str(), &w, &h, &nchan, 4);
    if (!data) {
        printf("Failed to load background image: %s\n", stbi_failure_reason());
        return false;
    }

    auto imageSize = w * h * nchan;

    printf("Decoded png at %s, %dx%d with %d channels: %d (%#x) bytes\n", path.c_str(), w, h, nchan, imageSize, imageSize);

    // wait for previous commands to complete
    s_queue.waitIdle();

    dk::ImageLayout layout;
    dk::ImageLayoutMaker{s_device}
        .setFlags(0)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(w, h)
        .initialize(layout);

    printf("Initialized layout: %#lx aligned to %#x\n", layout.getSize(), layout.getAlignment());

    auto memBlock = dk::MemBlockMaker{s_device, imgui::deko3d::align(imageSize, DK_MEMBLOCK_ALIGNMENT)}
        .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
        .create();

    s_imageMemBlock = dk::MemBlockMaker{s_device, imgui::deko3d::align(layout.getSize(), DK_MEMBLOCK_ALIGNMENT)}
        .setFlags(DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
        .create();

    printf("Created mem blocks of size %#x & %#x\n", memBlock.getSize(), s_imageMemBlock.getSize());

    std::memcpy(memBlock.getCpuAddr(), data, imageSize);

    dk::Image image;
    image.initialize(layout, s_imageMemBlock, 0);
    s_imageDescriptors[BG_IMAGE_ID].initialize(image);

    // copy texture to image
    dk::ImageView imageView(image);
    s_cmdBuf[0].copyBufferToImage({memBlock.getGpuAddr()},
        imageView,
        {0, 0, 0, static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 1});
    s_queue.submitCommands(s_cmdBuf[0].finishList());

    // initialize sampler descriptor
    s_samplerDescriptors[BG_SAMPLER_ID].initialize(
        dk::Sampler{}
            .setFilter(DkFilter_Linear, DkFilter_Linear)
            .setWrapMode(DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge));

    // wait for commands to complete before releasing memblocks
    s_queue.waitIdle();

    stbi_image_free(data);

    printf("Done uploading texture\n");

    return true;
}

} // namespace gui
