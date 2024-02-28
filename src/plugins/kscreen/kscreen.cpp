/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "kscreen.h"
#include "core/output.h"
#include "effect/effecthandler.h"
// KConfigSkeleton
#include "kscreenconfig.h"
#include <QDebug>

/**
 * How this effect works:
 *
 * Effect announces that it is around through property _KDE_KWIN_KSCREEN_SUPPORT on the root window.
 *
 * KScreen watches for this property and when it wants to adjust screens, KScreen goes
 * through the following protocol:
 * 1. KScreen sets the property value to 1
 * 2. Effect starts to fade out all windows
 * 3. When faded out the effect sets property value to 2
 * 4. KScreen adjusts the screens
 * 5. KScreen sets property value to 3
 * 6. Effect starts to fade in all windows again
 * 7. Effect sets back property value to 0
 *
 * The property has type 32 bits cardinal. To test it use:
 * xprop -root -f _KDE_KWIN_KSCREEN_SUPPORT 32c -set _KDE_KWIN_KSCREEN_SUPPORT 1
 *
 * The states are:
 * 0: normal
 * 1: fading out
 * 2: faded out
 * 3: fading in
 */

Q_LOGGING_CATEGORY(KWIN_KSCREEN, "kwin_effect_kscreen", QtWarningMsg)

namespace KWin
{

KscreenEffect::KscreenEffect()
    : Effect()
#if KWIN_BUILD_X11
    , m_atom(effects->waylandDisplay() ? XCB_ATOM_NONE : effects->announceSupportProperty("_KDE_KWIN_KSCREEN_SUPPORT", this))
#endif
{
    KscreenConfig::instance(effects->config());
#if KWIN_BUILD_X11
    if (!effects->waylandDisplay()) {
        connect(effects, &EffectsHandler::propertyNotify, this, &KscreenEffect::propertyNotify);
        connect(effects, &EffectsHandler::xcbConnectionChanged, this, [this]() {
            m_atom = effects->announceSupportProperty(QByteArrayLiteral("_KDE_KWIN_KSCREEN_SUPPORT"), this);
        });
    }
#endif
    reconfigure(ReconfigureAll);

    const QList<Output *> screens = effects->screens();
    for (auto screen : screens) {
        addScreen(screen);
    }
    connect(effects, &EffectsHandler::screenAdded, this, &KscreenEffect::addScreen);
    connect(effects, &EffectsHandler::screenRemoved, this, [this](KWin::Output *screen) {
        m_waylandStates.remove(screen);
    });
}

KscreenEffect::~KscreenEffect()
{
}

void KscreenEffect::addScreen(Output *screen)
{
    connect(screen, &Output::wakeUp, this, [this, screen] {
        auto &state = m_waylandStates[screen];
        state.m_timeLine.setDuration(std::chrono::milliseconds(animationTime<KscreenConfig>(250)));
        setState(state, StateFadingIn);
    });
    connect(screen, &Output::aboutToTurnOff, this, [this, screen](std::chrono::milliseconds dimmingIn) {
        auto &state = m_waylandStates[screen];
        state.m_timeLine.setDuration(dimmingIn);
        setState(state, StateFadingOut);
    });
}

void KscreenEffect::reconfigure(ReconfigureFlags flags)
{
    KscreenConfig::self()->read();
    m_xcbState.m_timeLine.setDuration(
        std::chrono::milliseconds(animationTime<KscreenConfig>(250)));
}

void KscreenEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (isScreenActive(data.screen)) {
        auto &state = !effects->waylandDisplay() ? m_xcbState : m_waylandStates[data.screen];
        m_currentScreen = data.screen;

        if (state.m_state == StateFadingIn || state.m_state == StateFadingOut) {
            state.m_timeLine.advance(presentTime);
            if (state.m_timeLine.done()) {
                switchState(state);
                if (state.m_state == StateNormal) {
                    m_waylandStates.remove(data.screen);
                }
            }
        }
    }

    effects->prePaintScreen(data, presentTime);
}

void KscreenEffect::postPaintScreen()
{
    if (isScreenActive(m_currentScreen)) {
        auto &state = !effects->waylandDisplay() ? m_xcbState : m_waylandStates[m_currentScreen];
        if (state.m_state == StateFadingIn || state.m_state == StateFadingOut) {
            effects->addRepaintFull();
        }
    }
    m_currentScreen = nullptr;
}

void KscreenEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto screen = w->screen();
    if (isScreenActive(screen)) {
        auto &state = !effects->waylandDisplay() ? m_xcbState : m_waylandStates[screen];
        if (state.m_state != StateNormal) {
            data.setTranslucent();
        }
    }
    effects->prePaintWindow(w, data, presentTime);
}

void KscreenEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto screen = w->screen();
    if (isScreenActive(screen)) {
        auto &state = !effects->waylandDisplay() ? m_xcbState : m_waylandStates[screen];
        // fade to black and fully opaque
        switch (state.m_state) {
        case StateFadingOut:
            data.setOpacity(data.opacity() + (1.0 - data.opacity()) * state.m_timeLine.value());
            data.multiplyBrightness(1.0 - state.m_timeLine.value());
            break;
        case StateFadedOut:
            data.multiplyOpacity(0.0);
            data.multiplyBrightness(0.0);
            break;
        case StateFadingIn:
            data.setOpacity(data.opacity() + (1.0 - data.opacity()) * (1.0 - state.m_timeLine.value()));
            data.multiplyBrightness(state.m_timeLine.value());
            break;
        default:
            // no adjustment
            break;
        }
    }
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

void KscreenEffect::setState(ScreenState &state, FadeOutState newState)
{
    if (state.m_state == newState) {
        return;
    }

    state.m_state = newState;
    state.m_timeLine.reset();
    effects->addRepaintFull();
}

#if KWIN_BUILD_X11
void KscreenEffect::propertyNotify(EffectWindow *window, long int atom)
{
    if (window || atom != m_atom || m_atom == XCB_ATOM_NONE) {
        return;
    }

    const QByteArray byteData = effects->readRootProperty(m_atom, XCB_ATOM_CARDINAL, 32);
    const uint32_t *data = byteData.isEmpty() ? nullptr : reinterpret_cast<const uint32_t *>(byteData.data());
    if (!data || data[0] >= LastState) { // Property was deleted
        if (data) {
            qCDebug(KWIN_KSCREEN) << "Incorrect Property state, immediate stop: " << data[0];
        }
        setState(m_xcbState, StateNormal);
        return;
    }

    setState(m_xcbState, FadeOutState(data[0]));
}
#endif

void KscreenEffect::switchState(ScreenState &state)
{
    long value = -1l;
    if (state.m_state == StateFadingOut) {
        state.m_state = StateFadedOut;
        value = 2l;
    } else if (state.m_state == StateFadingIn) {
        state.m_state = StateNormal;
        value = 0l;
    }
#if KWIN_BUILD_X11
    if (value != -1l && m_atom != XCB_ATOM_NONE) {
        xcb_change_property(effects->xcbConnection(), XCB_PROP_MODE_REPLACE, effects->x11RootWindow(), m_atom, XCB_ATOM_CARDINAL, 32, 1, &value);
    }
#endif
}

bool KscreenEffect::isActive() const
{
    return !m_waylandStates.isEmpty()
#if KWIN_BUILD_X11
        || (!effects->waylandDisplay() && m_atom && m_xcbState.m_state != StateNormal)
#endif
        ;
}

bool KscreenEffect::isScreenActive(Output *screen) const
{
    return m_waylandStates.contains(screen)
#if KWIN_BUILD_X11
        || (!effects->waylandDisplay() && m_atom && m_xcbState.m_state != StateNormal)
#endif
        ;
}

} // namespace KWin

#include "moc_kscreen.cpp"
