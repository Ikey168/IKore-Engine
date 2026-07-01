// Test for the interactive menu core - Milestone 9, #58.
//
// Verifies the headless menu model: focus navigation (skipping labels/disabled,
// wrapping), item activation and adjustment (button/toggle/slider/choice), direct
// focus (mouse hover), the menu stack (open/back/close, action routing), and that
// the same MenuAction stream drives everything (so keyboard/mouse/gamepad behave
// identically). Also checks menu items bound to the Settings store persist.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_menu_system.cpp -o test_menu_system

#include "core/Settings.h"
#include "ui/MenuSystem.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace IKore;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static void testFocusNavigation() {
    Menu menu("Main");
    menu.add(menuLabel("== Title =="));                    // 0: not selectable
    menu.add(menuButton("Play", [] {}));                   // 1
    MenuItem disabled = menuButton("Locked", [] {});
    disabled.enabled = false;
    menu.add(disabled);                                    // 2: not selectable
    menu.add(menuButton("Settings", [] {}));               // 3
    menu.add(menuButton("Quit", [] {}));                   // 4

    // Focus starts on the first selectable item (skips the label).
    CHECK(menu.focus() == 1);

    // Down skips the disabled row.
    menu.focusNext();
    CHECK(menu.focus() == 3);
    menu.focusNext();
    CHECK(menu.focus() == 4);
    menu.focusNext(); // wraps past the end back to the first selectable
    CHECK(menu.focus() == 1);

    // Up wraps the other way, still skipping non-selectable rows.
    menu.focusPrev();
    CHECK(menu.focus() == 4);
    menu.focusPrev();
    CHECK(menu.focus() == 3);

    // Direct focus (mouse hover) lands only on selectable rows.
    menu.setFocus(1);
    CHECK(menu.focus() == 1);
    menu.setFocus(0); // label -> ignored
    CHECK(menu.focus() == 1);
    menu.setFocus(2); // disabled -> ignored
    CHECK(menu.focus() == 1);
}

static void testButtonActivate() {
    int plays = 0;
    Menu menu("Main");
    menu.add(menuButton("Play", [&plays] { ++plays; }));
    menu.add(menuButton("Quit", [] {}));

    CHECK(menu.focus() == 0);
    CHECK(menu.activate());
    CHECK(plays == 1);
    menu.focusNext();
    CHECK(menu.activate()); // Quit has an (empty) action, still handled
    CHECK(plays == 1);
}

static void testToggleSliderChoice() {
    bool fullscreen = false;
    float volume = 0.5f;
    int quality = 1; // index into {Low, Medium, High}

    Menu menu("Settings");
    menu.add(menuToggle("Fullscreen", [&fullscreen] { return fullscreen; },
                        [&fullscreen](bool b) { fullscreen = b; }));
    menu.add(menuSlider("Volume", [&volume] { return volume; }, [&volume](float v) { volume = v; }, 0.0f,
                        1.0f, 0.1f));
    menu.add(menuChoice("Quality", {"Low", "Medium", "High"}, [&quality] { return quality; },
                        [&quality](int i) { quality = i; }));

    // Toggle: activate flips; left/right force off/on.
    CHECK(menu.focus() == 0);
    menu.activate();
    CHECK(fullscreen == true);
    menu.adjustLeft();
    CHECK(fullscreen == false);
    menu.adjustRight();
    CHECK(fullscreen == true);

    // Slider: right/left step and clamp to [min, max].
    menu.focusNext();
    CHECK(menu.focus() == 1);
    menu.adjustRight();
    CHECK(approx(volume, 0.6f));
    for (int i = 0; i < 10; ++i) menu.adjustRight();
    CHECK(approx(volume, 1.0f)); // clamped at max
    for (int i = 0; i < 20; ++i) menu.adjustLeft();
    CHECK(approx(volume, 0.0f)); // clamped at min
    CHECK(!menu.activate());     // Activate is a no-op on sliders

    // Choice: right/left clamp; activate cycles with wrap.
    menu.focusNext();
    CHECK(menu.focus() == 2);
    menu.adjustRight();
    CHECK(quality == 2);
    menu.adjustRight();
    CHECK(quality == 2); // clamped at last
    menu.adjustLeft();
    menu.adjustLeft();
    CHECK(quality == 0); // clamped at first
    menu.activate();     // wrap cycle: 0 -> 1
    CHECK(quality == 1);
}

static void testMenuStackNavigation() {
    Menu mainMenu("Main");
    Menu settingsMenu("Settings");
    MenuStack stack;

    // A "Settings" button opens the submenu; a "Back" button pops it.
    mainMenu.add(menuButton("Settings", [&] { stack.open(&settingsMenu); }));
    mainMenu.add(menuButton("Quit", [] {}));
    settingsMenu.add(menuButton("Back", [&] { stack.back(); }));

    CHECK(!stack.isOpen());
    stack.open(&mainMenu);
    CHECK(stack.isOpen());
    CHECK(stack.depth() == 1);
    CHECK(stack.current() == &mainMenu);

    // Route actions through the stack (this is what all input sources call).
    stack.handle(MenuAction::Activate); // focused item is "Settings" -> opens submenu
    CHECK(stack.depth() == 2);
    CHECK(stack.current() == &settingsMenu);

    stack.handle(MenuAction::Activate); // "Back" -> pops back to main
    CHECK(stack.depth() == 1);
    CHECK(stack.current() == &mainMenu);

    stack.handle(MenuAction::Back); // Back on the root pops it
    CHECK(!stack.isOpen());
    CHECK(stack.current() == nullptr);
    stack.handle(MenuAction::Down); // routing to an empty stack is safe
    CHECK(!stack.isOpen());

    stack.open(&mainMenu);
    stack.open(&settingsMenu);
    stack.closeAll();
    CHECK(!stack.isOpen());
}

static void testActionRoutingMovesFocus() {
    // The same MenuAction stream (whatever the source) drives navigation.
    Menu menu("Main");
    menu.add(menuButton("A", [] {}));
    menu.add(menuButton("B", [] {}));
    menu.add(menuButton("C", [] {}));
    MenuStack stack;
    stack.open(&menu);

    CHECK(menu.focus() == 0);
    stack.handle(MenuAction::Down);
    CHECK(menu.focus() == 1);
    stack.handle(MenuAction::Down);
    CHECK(menu.focus() == 2);
    stack.handle(MenuAction::Up);
    CHECK(menu.focus() == 1);
}

static void testSettingsBoundMenuPersists() {
    // A settings menu bound to the Settings store: toggling a menu item changes the
    // stored value, which then serializes - i.e. the change persists.
    Settings settings;
    settings.setBool("vsync", false);

    Menu menu("Settings");
    menu.add(menuToggle("VSync", [&settings] { return settings.getBool("vsync"); },
                        [&settings](bool b) { settings.setBool("vsync", b); }));

    CHECK(settings.getBool("vsync") == false);
    menu.activate(); // flips the toggle -> writes into the store
    CHECK(settings.getBool("vsync") == true);

    const std::string serialized = settings.serialize();
    Settings reloaded;
    reloaded.load(serialized);
    CHECK(reloaded.getBool("vsync") == true); // survived the round trip
}

int main() {
    testFocusNavigation();
    testButtonActivate();
    testToggleSliderChoice();
    testMenuStackNavigation();
    testActionRoutingMovesFocus();
    testSettingsBoundMenuPersists();

    if (g_failures == 0) {
        std::printf("All menu system tests passed.\n");
        return 0;
    }
    std::printf("%d menu system test(s) failed.\n", g_failures);
    return 1;
}
