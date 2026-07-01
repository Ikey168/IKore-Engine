#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @file MenuSystem.h
 * @brief Interactive menu model: items, navigation, and a screen stack (issue #58).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless, renderer- and
 * input-agnostic core behind the main / pause / settings menus: a Menu is a titled
 * list of MenuItems (buttons, toggles, sliders, choices, labels) with a focus
 * cursor; a MenuStack pushes and pops menus so submenus (Settings) layer over their
 * parent. All input is funnelled through a single MenuAction enum, so keyboard,
 * mouse, and gamepad produce the same actions and navigation is identical for each.
 * Item values bind to live getters/setters (e.g. into the Settings store), so a
 * toggle or slider edits the backing value directly.
 *
 * The ImGui panel in DebugUI draws the current menu (centered via the HUD framework
 * #55) and maps key / pad / mouse events to MenuAction. Keeping the navigation and
 * item logic here makes it unit-testable without a GL context. Header-only, std only.
 */
namespace IKore {

/// A source-agnostic navigation action. The engine maps keys, gamepad buttons, and
/// mouse events onto these, so all input types drive the menus identically.
enum class MenuAction { Up, Down, Left, Right, Activate, Back };

enum class MenuItemType { Button, Toggle, Slider, Choice, Label };

/**
 * @brief One row of a menu. Only the accessors matching @ref type are used.
 *
 * A plain aggregate; use the menu* helpers to build the common cases. Buttons run
 * onActivate; toggles/sliders/choices read and write their bound value live.
 */
struct MenuItem {
    std::string label;
    MenuItemType type{MenuItemType::Button};
    bool enabled{true};

    std::function<void()> onActivate; // Button

    std::function<bool()> getBool; // Toggle
    std::function<void(bool)> setBool;

    std::function<float()> getFloat; // Slider
    std::function<void(float)> setFloat;
    float minValue{0.0f};
    float maxValue{1.0f};
    float step{0.1f};

    std::function<int()> getChoice; // Choice (index into options)
    std::function<void(int)> setChoice;
    std::vector<std::string> options;

    /// Labels and disabled rows cannot take focus.
    bool selectable() const { return enabled && type != MenuItemType::Label; }
};

inline MenuItem menuLabel(std::string text) {
    MenuItem item;
    item.label = std::move(text);
    item.type = MenuItemType::Label;
    return item;
}

inline MenuItem menuButton(std::string label, std::function<void()> onActivate) {
    MenuItem item;
    item.label = std::move(label);
    item.type = MenuItemType::Button;
    item.onActivate = std::move(onActivate);
    return item;
}

inline MenuItem menuToggle(std::string label, std::function<bool()> get, std::function<void(bool)> set) {
    MenuItem item;
    item.label = std::move(label);
    item.type = MenuItemType::Toggle;
    item.getBool = std::move(get);
    item.setBool = std::move(set);
    return item;
}

inline MenuItem menuSlider(std::string label, std::function<float()> get, std::function<void(float)> set,
                           float minValue, float maxValue, float step) {
    MenuItem item;
    item.label = std::move(label);
    item.type = MenuItemType::Slider;
    item.getFloat = std::move(get);
    item.setFloat = std::move(set);
    item.minValue = minValue;
    item.maxValue = maxValue;
    item.step = step;
    return item;
}

inline MenuItem menuChoice(std::string label, std::vector<std::string> options,
                           std::function<int()> get, std::function<void(int)> set) {
    MenuItem item;
    item.label = std::move(label);
    item.type = MenuItemType::Choice;
    item.options = std::move(options);
    item.getChoice = std::move(get);
    item.setChoice = std::move(set);
    return item;
}

/**
 * @brief A titled menu screen: a list of items with a focus cursor.
 *
 * Focus starts on the first selectable item and skips labels/disabled rows as it
 * moves (wrapping at the ends). activate() triggers the focused item; adjustLeft/
 * adjustRight change sliders, choices, and toggles.
 */
class Menu {
public:
    explicit Menu(std::string title = std::string()) : m_title(std::move(title)) {}

    const std::string& title() const { return m_title; }
    void setTitle(std::string title) { m_title = std::move(title); }

    void add(MenuItem item) {
        m_items.push_back(std::move(item));
        if (m_focus < 0 && m_items.back().selectable()) {
            m_focus = static_cast<int>(m_items.size()) - 1;
        }
    }

    const std::vector<MenuItem>& items() const { return m_items; }
    std::size_t count() const { return m_items.size(); }

    int focus() const { return m_focus; }
    bool hasFocus() const { return m_focus >= 0 && m_focus < static_cast<int>(m_items.size()); }

    /// Set focus directly (e.g. mouse hover). Ignored if the row is not selectable.
    void setFocus(int index) {
        if (index >= 0 && index < static_cast<int>(m_items.size()) && m_items[index].selectable()) {
            m_focus = index;
        }
    }

    void focusNext() { moveFocus(+1); }
    void focusPrev() { moveFocus(-1); }

    /// Trigger the focused item: run a button, flip a toggle, cycle a choice.
    bool activate() {
        if (!hasFocus()) return false;
        const MenuItem& item = m_items[static_cast<std::size_t>(m_focus)];
        switch (item.type) {
            case MenuItemType::Button:
                if (item.onActivate) {
                    item.onActivate();
                    return true;
                }
                return false;
            case MenuItemType::Toggle:
                if (item.getBool && item.setBool) {
                    item.setBool(!item.getBool());
                    return true;
                }
                return false;
            case MenuItemType::Choice:
                return cycleChoice(item, +1, /*wrap=*/true);
            case MenuItemType::Slider: // sliders are changed with left/right
            case MenuItemType::Label:
                return false;
        }
        return false;
    }

    bool adjustRight() { return adjust(+1); }
    bool adjustLeft() { return adjust(-1); }

private:
    void moveFocus(int dir) {
        const int n = static_cast<int>(m_items.size());
        if (n == 0 || m_focus < 0) return;
        int idx = m_focus;
        for (int step = 0; step < n; ++step) {
            idx = (idx + dir + n) % n;
            if (m_items[static_cast<std::size_t>(idx)].selectable()) {
                m_focus = idx;
                return;
            }
        }
    }

    bool adjust(int dir) {
        if (!hasFocus()) return false;
        const MenuItem& item = m_items[static_cast<std::size_t>(m_focus)];
        switch (item.type) {
            case MenuItemType::Slider:
                if (item.getFloat && item.setFloat) {
                    float v = item.getFloat() + static_cast<float>(dir) * item.step;
                    if (v < item.minValue) v = item.minValue;
                    if (v > item.maxValue) v = item.maxValue;
                    item.setFloat(v);
                    return true;
                }
                return false;
            case MenuItemType::Choice:
                return cycleChoice(item, dir, /*wrap=*/false);
            case MenuItemType::Toggle:
                if (item.setBool) {
                    item.setBool(dir > 0);
                    return true;
                }
                return false;
            default:
                return false;
        }
    }

    static bool cycleChoice(const MenuItem& item, int dir, bool wrap) {
        if (!item.getChoice || !item.setChoice || item.options.empty()) return false;
        const int n = static_cast<int>(item.options.size());
        int next = item.getChoice() + dir;
        if (wrap) {
            next = (next % n + n) % n;
        } else {
            if (next < 0) next = 0;
            if (next >= n) next = n - 1;
        }
        item.setChoice(next);
        return true;
    }

    std::string m_title;
    std::vector<MenuItem> m_items;
    int m_focus{-1};
};

/**
 * @brief A stack of open menus. The top menu receives navigation; Back pops it.
 *
 * Menus are owned elsewhere (the stack holds non-owning pointers), so a button can
 * push a submenu (open(&settings)) or return to the parent (back()). handle()
 * routes a MenuAction to the current menu, which is all the engine needs to drive
 * menus from any input source.
 */
class MenuStack {
public:
    void open(Menu* menu) {
        if (menu != nullptr) m_stack.push_back(menu);
    }
    void back() {
        if (!m_stack.empty()) m_stack.pop_back();
    }
    void closeAll() { m_stack.clear(); }

    bool isOpen() const { return !m_stack.empty(); }
    std::size_t depth() const { return m_stack.size(); }
    Menu* current() const { return m_stack.empty() ? nullptr : m_stack.back(); }

    void handle(MenuAction action) {
        Menu* cur = current();
        if (cur == nullptr) return;
        switch (action) {
            case MenuAction::Up:
                cur->focusPrev();
                break;
            case MenuAction::Down:
                cur->focusNext();
                break;
            case MenuAction::Left:
                cur->adjustLeft();
                break;
            case MenuAction::Right:
                cur->adjustRight();
                break;
            case MenuAction::Activate:
                cur->activate();
                break;
            case MenuAction::Back:
                back();
                break;
        }
    }

private:
    std::vector<Menu*> m_stack;
};

} // namespace IKore
