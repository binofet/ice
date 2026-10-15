#include "Platform/icHost.h"
#include <algorithm>
#include <cmath>

bool icHost::SetState(icAppState state) {
    if (m_state == icAppState::Stopped) return false;
    if (state == icAppState::Stopped) { Stop(); return true; }
    if (state == m_state) return true;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_acceptInput = state == icAppState::Active;
        m_pending.clear();
        m_overflow = false;
    }
    if (state != icAppState::Active) CancelInputs();
    m_state = state;
    m_clockValid = false;
    m_client.OnAppState(state);
    return true;
}
icSurfaceID icHost::AddSurface(icSurface surface) {
    if (m_state == icAppState::Stopped || surface.width < 0 || surface.height < 0 ||
        !std::isfinite(surface.scale) || surface.scale <= 0) return 0;
    surface.id = m_nextSurface++;
    m_surfaces[surface.id] = surface;
    m_client.OnSurfaceAdded(surface);
    return surface.id;
}
bool icHost::ResizeSurface(icSurfaceID id,int width,int height,float scale) {
    auto found = m_surfaces.find(id);
    if (found == m_surfaces.end() || width < 0 || height < 0 || !std::isfinite(scale) || scale <= 0) return false;
    auto& surface = found->second;
    if (surface.width == width && surface.height == height && surface.scale == scale) return true;
    surface.width = width; surface.height = height; surface.scale = scale;
    m_client.OnSurfaceChanged(surface);
    return true;
}
bool icHost::SetSurfaceVisible(icSurfaceID id, bool visible) {
    auto found = m_surfaces.find(id);
    if (found == m_surfaces.end()) return false;
    if (found->second.visible == visible) return true;
    found->second.visible = visible;
    if (!visible) CancelInputs(0,id);
    m_client.OnSurfaceChanged(found->second);
    return true;
}
bool icHost::RemoveSurface(icSurfaceID id) {
    auto found = m_surfaces.find(id);
    if (found == m_surfaces.end()) return false;
    CancelInputs(0,id);
    m_client.OnSurfaceRemoved(found->second);
    m_surfaces.erase(found);
    return true;
}
icDeviceID icHost::AddDevice(std::uint32_t capabilities) {
    if (m_state == icAppState::Stopped || !capabilities) return 0;
    icInputDevice device; device.id = m_nextDevice++; device.capabilities = capabilities;
    m_devices[device.id] = device;
    m_client.OnDeviceAdded(device);
    return device.id;
}
bool icHost::RemoveDevice(icDeviceID id) {
    if (!m_devices.count(id)) return false;
    CancelInputs(id);
    m_devices.erase(id);
    m_client.OnDeviceRemoved(id);
    return true;
}
bool icHost::PostInput(const icDeviceInput& input) {
    if (!std::isfinite(input.seconds) || input.seconds < 0 || !std::isfinite(input.x) ||
        !std::isfinite(input.y) || !std::isfinite(input.z) || !std::isfinite(input.w)) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_acceptInput) return false;
    if (m_pending.size() == 1024) { ++m_dropped; m_overflow = true; return false; }
    m_pending.push_back(input);
    return true;
}
void icHost::CancelInputs(icDeviceID device,icSurfaceID surface) {
    for (auto i = m_held.begin(); i != m_held.end();) {
        if ((!device || i->device == device) && (!surface || i->surface == surface)) {
            auto event = *i; event.phase = icInputPhase::Cancel;
            m_client.OnInput(event);
            i = m_held.erase(i);
        } else ++i;
    }
}
void icHost::DrainInput() {
    std::deque<icDeviceInput> pending;
    bool overflow;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pending.swap(m_pending);
        overflow = m_overflow; m_overflow = false;
    }
    // A dropped release must never leave a key/touch held indefinitely.
    if (overflow) { CancelInputs(); return; }
    for (auto event : pending) {
        auto device = m_devices.find(event.device);
        unsigned kind = static_cast<unsigned>(event.kind);
        if (kind > static_cast<unsigned>(icInputKind::Attitude) || device == m_devices.end() ||
            !(device->second.capabilities & icInputCapability(event.kind)) ||
            (event.surface && (!m_surfaces.count(event.surface) || !m_surfaces.at(event.surface).visible))) continue;
        bool held = event.kind == icInputKind::Pointer || event.kind == icInputKind::Key;
        auto same = [&](const icDeviceInput& other) {
            return other.device == event.device && other.surface == event.surface &&
                   other.contact == event.contact && other.kind == event.kind;
        };
        if (held) {
            auto found = std::find_if(m_held.begin(),m_held.end(),same);
            if (event.phase == icInputPhase::Begin) {
                if (found != m_held.end()) continue;
                m_held.push_back(event);
            } else if (event.phase == icInputPhase::Change) {
                if (found == m_held.end()) continue;
                *found = event;
            } else if (event.phase == icInputPhase::End || event.phase == icInputPhase::Cancel) {
                if (found == m_held.end()) continue;
                m_held.erase(found);
            } else continue;
        } else if (event.phase != icInputPhase::Sample) continue;
        m_client.OnInput(event);
    }
}
void icHost::Frame(double seconds) {
    if (m_state != icAppState::Active || !std::isfinite(seconds) || seconds < 0) return;
    DrainInput();
    double dt = m_clockValid ? std::max(0.0,std::min(.1,seconds-m_previous)) : 0;
    // Never move the time baseline backwards if a platform supplies a stale tick.
    if (!m_clockValid || seconds >= m_previous) m_previous = seconds;
    m_clockValid = true;
    m_client.OnUpdate(dt);
    for (const auto& entry : m_surfaces)
        if (entry.second.visible && entry.second.width > 0 && entry.second.height > 0) m_client.OnRender(entry.second);
}
void icHost::Stop() {
    if (m_state == icAppState::Stopped) return;
    SetState(icAppState::Inactive);
    while (!m_surfaces.empty()) RemoveSurface(m_surfaces.begin()->first);
    while (!m_devices.empty()) RemoveDevice(m_devices.begin()->first);
    m_state = icAppState::Stopped;
    m_client.OnAppState(m_state);
}
