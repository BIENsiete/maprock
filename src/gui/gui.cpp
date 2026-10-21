#include "gui.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

#include "../export_pipeline.hpp"
#include "../map_image.hpp"

namespace maprock {

namespace {

struct GuiState {
  bool loaded = false;
  Prepared prepared;
  MapImage map;
  GLuint mapTex = 0;

  char worldPath[1024] = "";
  char rpPath[1024] = "";

  int selMinX = 0, selMinZ = 0, selMaxX = 0, selMaxZ = 0;
  int yMin = 0, yMax = 0;
  bool whole = true;
  int fmtSel = 0;  // 0 = obj, 1 = gltf
  char format[8] = "obj";
  char output[512] = "export";
  char status[512] = "";
  char loadErr[512] = "";

  bool dragging = false;
  float dragX0 = 0, dragY0 = 0, dragX1 = 0, dragY1 = 0;
};

void glfwErrorCallback(int, const char* desc) {
  std::fprintf(stderr, "GLFW error: %s\n", desc);
}

bool uploadTexture(GLuint& tex, const MapImage& map) {
  if (map.w <= 0 || map.h <= 0) return false;
  if (tex == 0) glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);  // GL_CLAMP_TO_EDGE
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, map.w, map.h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               map.rgba.data());
  return true;
}

#ifdef _WIN32
bool pickFolder(char* out, size_t outSize) {
  bool ok = false;
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool uninit = SUCCEEDED(hr);
  IFileDialog* dlg = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&dlg)))) {
    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts))) {
      dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    if (SUCCEEDED(dlg->Show(nullptr))) {
      IShellItem* item = nullptr;
      if (SUCCEEDED(dlg->GetResult(&item))) {
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
          int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
          if (n > 0 && static_cast<size_t>(n) <= outSize) {
            WideCharToMultiByte(CP_UTF8, 0, path, -1, out, static_cast<int>(outSize),
                                nullptr, nullptr);
            ok = true;
          }
          CoTaskMemFree(path);
        }
        item->Release();
      }
    }
    dlg->Release();
  }
  if (uninit) CoUninitialize();
  return ok;
}
#else
bool pickFolder(char*, size_t) { return false; }
#endif

// Loads the world into `st` (no GL calls; texture upload is deferred).
bool loadInto(GuiState& st) {
  st.loadErr[0] = 0;
  std::string err = prepare(st.prepared, st.worldPath, st.rpPath, true, 0, 0, 0, 0, 0, 0,
                            [](float f, const std::string& s) {
                              (void)f;
                              (void)s;
                            });
  if (!err.empty()) {
    std::snprintf(st.loadErr, sizeof(st.loadErr), "%s", err.c_str());
    st.loaded = false;
    return false;
  }
  buildMapImage(st.prepared.world, st.prepared.colors, 2048, st.map);
  st.selMinX = st.prepared.world.minX;
  st.selMaxX = st.prepared.world.maxX;
  st.selMinZ = st.prepared.world.minZ;
  st.selMaxZ = st.prepared.world.maxZ;
  st.yMin = st.prepared.world.minY;
  st.yMax = st.prepared.world.maxY;
  st.loaded = true;
  return true;
}

void doExport(GuiState& st) {
  Region region;
  const Region* rp = nullptr;
  if (!st.whole) {
    region.minX = st.selMinX;
    region.minY = st.yMin;
    region.minZ = st.selMinZ;
    region.maxX = st.selMaxX;
    region.maxY = st.yMax;
    region.maxZ = st.selMaxZ;
    rp = &region;
  }
  std::string base = st.output;
  std::string err = exportPrepared(st.prepared, base, st.format, rp);
  if (err.empty()) {
    std::snprintf(st.status, sizeof(st.status), "Exported to %s.%s", base.c_str(), st.format);
  } else {
    std::snprintf(st.status, sizeof(st.status), "Error: %s", err.c_str());
  }
}

void renderBrowse(GuiState& st, bool& texDirty) {
  ImGui::TextUnformatted("Load a Minecraft Bedrock world");
  ImGui::Separator();
  ImGui::TextUnformatted("World folder (contains 'level.dat' or a 'db' folder):");
  ImGui::InputText("##world", st.worldPath, sizeof(st.worldPath));
  ImGui::SameLine();
  if (ImGui::Button("Browse...")) {
    if (pickFolder(st.worldPath, sizeof(st.worldPath))) texDirty = false;
  }
  ImGui::TextUnformatted("Resource pack (optional, bedrock-samples 'resource_pack'):");
  ImGui::InputText("##rp", st.rpPath, sizeof(st.rpPath));
  ImGui::SameLine();
  if (ImGui::Button("Browse ##rp")) {
    if (pickFolder(st.rpPath, sizeof(st.rpPath))) texDirty = false;
  }

  if (ImGui::Button("Load world", ImVec2(-1, 0))) {
    if (loadInto(st)) texDirty = true;
  }
  if (st.loadErr[0]) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", st.loadErr);
  }
  ImGui::Separator();
  ImGui::TextWrapped(
      "Tip: you can also pass the world as a command-line argument:\n"
      "  maprock-gui \"path/to/world\" \"path/to/resource_pack\"");
}

void renderMain(GuiState& st) {
  ImGui::BeginChild("panel", ImVec2(340, 0), false);
  ImGui::Text("World: %s", st.worldPath);
  ImGui::Separator();
  ImGui::Text("Bounds (blocks)");
  ImGui::Text("  X: %d .. %d", st.prepared.world.minX, st.prepared.world.maxX);
  ImGui::Text("  Y: %d .. %d", st.prepared.world.minY, st.prepared.world.maxY);
  ImGui::Text("  Z: %d .. %d", st.prepared.world.minZ, st.prepared.world.maxZ);
  ImGui::Text("Blocks: %zu", st.prepared.world.blocks.size());
  ImGui::Separator();

  ImGui::Checkbox("Export whole world", &st.whole);

  if (!st.whole) {
    ImGui::Text("Selection (X/Z from the map)");
    ImGui::DragInt("min X", &st.selMinX, 1.0f, st.prepared.world.minX, st.prepared.world.maxX);
    ImGui::DragInt("max X", &st.selMaxX, 1.0f, st.prepared.world.minX, st.prepared.world.maxX);
    ImGui::DragInt("min Z", &st.selMinZ, 1.0f, st.prepared.world.minZ, st.prepared.world.maxZ);
    ImGui::DragInt("max Z", &st.selMaxZ, 1.0f, st.prepared.world.minZ, st.prepared.world.maxZ);
  }

  ImGui::Text("Height range");
  ImGui::DragInt("min Y", &st.yMin, 1.0f, st.prepared.world.minY, st.prepared.world.maxY);
  ImGui::DragInt("max Y", &st.yMax, 1.0f, st.prepared.world.minY, st.prepared.world.maxY);

  ImGui::Separator();
  ImGui::Text("Export");
  ImGui::RadioButton("OBJ", &st.fmtSel, 0);
  ImGui::SameLine();
  ImGui::RadioButton("glTF", &st.fmtSel, 1);
  std::strcpy(st.format, st.fmtSel == 0 ? "obj" : "gltf");
  ImGui::InputText("Output", st.output, sizeof(st.output));

  if (ImGui::Button("Export", ImVec2(-1, 0))) doExport(st);
  if (st.status[0]) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", st.status);

  if (ImGui::Button("Load a different world", ImVec2(-1, 0))) {
    st.loaded = false;
    st.mapTex = 0;
  }

  ImGui::Text("\nDrag on the map to select a region.");
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("map", ImVec2(0, 0), false);
  ImVec2 avail = ImGui::GetContentRegionAvail();
  ImVec2 uv0(0, 0), uv1(1, 1);
  ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(st.mapTex)), avail, uv0, uv1);
  ImVec2 imgEnd = ImGui::GetItemRectMax();
  ImVec2 imgStart = ImGui::GetItemRectMin();

  if (ImGui::IsItemHovered()) {
    ImVec2 mp = ImGui::GetMousePos();
    if (ImGui::IsMouseClicked(0)) {
      st.dragging = true;
      st.dragX0 = st.dragX1 = mp.x;
      st.dragY0 = st.dragY1 = mp.y;
    }
    if (st.dragging && ImGui::IsMouseDragging(0)) {
      st.dragX1 = mp.x;
      st.dragY1 = mp.y;
    }
  }
  if (st.dragging && ImGui::IsMouseReleased(0)) {
    st.dragging = false;
    float w = imgEnd.x - imgStart.x;
    float h = imgEnd.y - imgStart.y;
    auto toWorld = [&](float sx, float sy, int& wx, int& wz) {
      float nx = std::clamp((sx - imgStart.x) / w, 0.0f, 1.0f);
      float ny = std::clamp((sy - imgStart.y) / h, 0.0f, 1.0f);
      int px = static_cast<int>(nx * st.map.w);
      int py = static_cast<int>(ny * st.map.h);
      wx = st.map.offsetX + px * st.map.scale;
      wz = st.map.offsetZ + py * st.map.scale;
    };
    int x0, z0, x1, z1;
    toWorld(std::min(st.dragX0, st.dragX1), std::min(st.dragY0, st.dragY1), x0, z0);
    toWorld(std::max(st.dragX0, st.dragX1), std::max(st.dragY0, st.dragY1), x1, z1);
    st.selMinX = std::max(st.prepared.world.minX, x0);
    st.selMinZ = std::max(st.prepared.world.minZ, z0);
    st.selMaxX = std::min(st.prepared.world.maxX, x1);
    st.selMaxZ = std::min(st.prepared.world.maxZ, z1);
    st.whole = false;
  }

  if (!st.whole || st.dragging) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = imgEnd.x - imgStart.x;
    float h = imgEnd.y - imgStart.y;
    auto worldToScreen = [&](int wx, int wz, float& sx, float& sy) {
      float nx = (wx - st.map.offsetX) / static_cast<float>(st.map.w * st.map.scale);
      float ny = (wz - st.map.offsetZ) / static_cast<float>(st.map.h * st.map.scale);
      sx = imgStart.x + nx * w;
      sy = imgStart.y + ny * h;
    };
    float rx0, ry0, rx1, ry1;
    if (st.dragging) {
      rx0 = std::min(st.dragX0, st.dragX1);
      ry0 = std::min(st.dragY0, st.dragY1);
      rx1 = std::max(st.dragX0, st.dragX1);
      ry1 = std::max(st.dragY0, st.dragY1);
    } else {
      worldToScreen(st.selMinX, st.selMinZ, rx0, ry0);
      worldToScreen(st.selMaxX + st.map.scale, st.selMaxZ + st.map.scale, rx1, ry1);
    }
    dl->AddRect(ImVec2(rx0, ry0), ImVec2(rx1, ry1), IM_COL32(255, 64, 64, 255), 0.0f, 0, 2.0f);
  }

  ImGui::EndChild();
}

}  // namespace

int runGui(const std::string& worldPath, const std::string& resourcePack) {
  GuiState st;
  std::snprintf(st.worldPath, sizeof(st.worldPath), "%s", worldPath.c_str());
  std::snprintf(st.rpPath, sizeof(st.rpPath), "%s", resourcePack.c_str());

  if (!worldPath.empty()) {
    loadInto(st);
  }

  glfwSetErrorCallback(glfwErrorCallback);
  if (!glfwInit()) {
    std::fprintf(stderr, "Failed to initialize GLFW\n");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
  GLFWwindow* window = glfwCreateWindow(1280, 800, "maprock", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "Failed to create window\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 150");

  bool texDirty = st.loaded;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (texDirty) {
      uploadTexture(st.mapTex, st.map);
      texDirty = false;
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("maprock", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (st.loaded) {
      renderMain(st);
    } else {
      renderBrowse(st, texDirty);
    }

    ImGui::End();

    ImGui::Render();
    int dw, dh;
    glfwGetFramebufferSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

}  // namespace maprock
