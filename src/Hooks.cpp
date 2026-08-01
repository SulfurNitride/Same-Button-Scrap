#include "PCH.h"

#include "Hooks.h"

namespace
{
    constexpr std::size_t kPerformInputProcessingIndex{ 0 };
    constexpr std::string_view kScrapButtonEvent{ "XButton" };
    constexpr std::string_view kAcceptEvent{ "Accept" };

    using InputProcessor = void (*)(
        RE::UI*,
        const RE::InputEvent*);
    using ShowConfirmHandler = void (*)(
        RE::ExamineMenu*,
        RE::ExamineConfirmMenu::InitData*,
        RE::ExamineConfirmMenu::ICallback*);

    InputProcessor originalInputProcessor{ nullptr };
    ShowConfirmHandler originalShowConfirmMenu{ nullptr };
    void* showConfirmTarget{ nullptr };
    std::atomic<bool> workshopScrapButtonReleased{ true };
    std::atomic<bool> workshopScrapPending{ false };
    std::atomic<bool> workbenchScrapButtonReleased{ true };
    std::atomic<bool> workbenchScrapConfirmation{ false };
    std::atomic<bool> acceptReleasePending{ false };

    REL::ID RuntimeID(
        const std::uint64_t og,
        const std::uint64_t ng,
        const std::uint64_t ae)
    {
        const auto runtime =
            REX::FModule::GetExecutingModule().GetFileVersion();
        if (runtime < F4SE::RUNTIME_1_10_980) {
            return REL::ID{ og };
        }
        if (runtime < F4SE::RUNTIME_1_11_137) {
            return REL::ID{ ng };
        }
        return REL::ID{ ae };
    }

    RE::ControlMap* GetControlMap()
    {
        static REL::Relocation<RE::ControlMap**> singleton{
            RuntimeID(325206, 2692014, 4799307)
        };
        return *singleton;
    }

    bool IsMenuOpen(const RE::IMenu* menu)
    {
        return menu && menu->OnStack();
    }

    bool IsScrapButtonEvent(const RE::ButtonEvent* event)
    {
        if (!event) {
            return false;
        }
        if (event->QRawUserEvent() == kScrapButtonEvent) {
            return true;
        }

        const auto device = event->device.get();
        if (device < RE::INPUT_DEVICE::kKeyboard ||
            device >= RE::INPUT_DEVICE::kSupported) {
            return false;
        }

        const auto* controlMap = GetControlMap();
        if (!controlMap) {
            return false;
        }

        constexpr RE::UserEvents::INPUT_CONTEXT_ID contexts[]{
            RE::UserEvents::INPUT_CONTEXT_ID::kWorkshop,
            RE::UserEvents::INPUT_CONTEXT_ID::kBasicMenuNav
        };
        for (const auto context : contexts) {
            const auto mappedKey = controlMap->GetMappedKey(
                kScrapButtonEvent,
                device,
                context);
            if (mappedKey != RE::ControlMap::kInvalid &&
                event->QIDCode() == mappedKey) {
                return true;
            }
        }
        return false;
    }

    void UIPerformInputProcessing(
        RE::UI* self,
        const RE::InputEvent* queueHead)
    {
        auto workshopMenu = self->GetMenu<RE::WorkshopMenu>();
        auto examineMenu = self->GetMenu<RE::ExamineMenu>();
        auto examineConfirmMenu =
            self->GetMenu<RE::ExamineConfirmMenu>();

        const bool workshopOpen = IsMenuOpen(workshopMenu.get());
        const bool workshopDialogOpen =
            workshopOpen &&
            IsMenuOpen(examineConfirmMenu.get());
        const bool workshopConfirmation =
            workshopDialogOpen &&
            workshopScrapPending.load(std::memory_order_acquire);
        const bool workbenchOpen = IsMenuOpen(examineMenu.get());
        const bool workbenchConfirmation =
            IsMenuOpen(examineConfirmMenu.get()) &&
            workbenchScrapConfirmation.load(std::memory_order_acquire);

        RE::ButtonEvent* remappedEvent{ nullptr };

        for (auto* input = queueHead; input; input = input->next) {
            const auto* button = input->As<RE::ButtonEvent>();
            if (!IsScrapButtonEvent(button)) {
                continue;
            }

            bool forwardAsAccept{ false };
            const bool isPendingAcceptRelease =
                button->QReleased() &&
                acceptReleasePending.exchange(
                    false,
                    std::memory_order_acq_rel);

            if (isPendingAcceptRelease) {
                forwardAsAccept =
                    IsMenuOpen(examineConfirmMenu.get());
                workshopScrapButtonReleased.store(
                    true,
                    std::memory_order_release);
                workbenchScrapButtonReleased.store(
                    true,
                    std::memory_order_release);
                workshopScrapPending.store(
                    false,
                    std::memory_order_release);
                workbenchScrapConfirmation.store(
                    false,
                    std::memory_order_release);
            } else if (workshopConfirmation) {
                if (button->QReleased()) {
                    workshopScrapButtonReleased.store(
                        true,
                        std::memory_order_release);
                } else if (button->QJustPressed()) {
                    forwardAsAccept =
                        workshopScrapButtonReleased.exchange(
                            false,
                            std::memory_order_acq_rel);
                }
            } else if (workshopOpen) {
                if (button->QJustPressed()) {
                    workshopScrapButtonReleased.store(
                        false,
                        std::memory_order_release);
                    workshopScrapPending.store(
                        true,
                        std::memory_order_release);
                } else if (button->QReleased()) {
                    workshopScrapButtonReleased.store(
                        true,
                        std::memory_order_release);
                }
            }

            if (workbenchConfirmation) {
                if (button->QReleased()) {
                    workbenchScrapButtonReleased.store(
                        true,
                        std::memory_order_release);
                } else if (button->QJustPressed()) {
                    forwardAsAccept =
                        workbenchScrapButtonReleased.exchange(
                            false,
                            std::memory_order_acq_rel) ||
                        forwardAsAccept;
                }
            } else if (workbenchOpen) {
                if (button->QJustPressed()) {
                    workbenchScrapButtonReleased.store(
                        false,
                        std::memory_order_release);
                } else if (button->QReleased()) {
                    workbenchScrapButtonReleased.store(
                        true,
                        std::memory_order_release);
                }
            }

            if (forwardAsAccept && !remappedEvent) {
                remappedEvent = const_cast<RE::ButtonEvent*>(button);
                if (button->QJustPressed()) {
                    acceptReleasePending.store(
                        true,
                        std::memory_order_release);
                }
                spdlog::debug(
                    "Remapping scrap {} to Accept",
                    button->QJustPressed() ? "press" : "release");
            }
        }

        if (remappedEvent) {
            const RE::BSFixedString originalUserEvent{
                remappedEvent->strUserEvent
            };
            const bool originalDisabled = remappedEvent->disabled;
            remappedEvent->strUserEvent = kAcceptEvent;
            remappedEvent->disabled = false;

            originalInputProcessor(self, queueHead);

            remappedEvent->strUserEvent = originalUserEvent;
            remappedEvent->disabled = originalDisabled;
        } else {
            originalInputProcessor(self, queueHead);
        }
    }

    void ShowConfirmMenu(
        RE::ExamineMenu* menu,
        RE::ExamineConfirmMenu::InitData* data,
        RE::ExamineConfirmMenu::ICallback* callback)
    {
        const bool isScrap =
            data &&
            data->confirmType ==
                RE::ExamineConfirmMenu::CONFIRM_TYPE::kScrap;
        workbenchScrapConfirmation.store(
            isScrap,
            std::memory_order_release);
        originalShowConfirmMenu(menu, data, callback);
    }

    bool InstallUIInputHook()
    {
        REL::Relocation<std::uintptr_t> uiVtable{
            RE::VTABLE::UI[0]
        };
        originalInputProcessor =
            reinterpret_cast<InputProcessor>(
                uiVtable.write_vfunc(
                    kPerformInputProcessingIndex,
                    &UIPerformInputProcessing));
        return originalInputProcessor != nullptr;
    }

    bool InstallShowConfirmHook()
    {
        REL::Relocation<std::uintptr_t> target{
            RuntimeID(443081, 2223081, 2223081)
        };
        showConfirmTarget = reinterpret_cast<void*>(target.address());
        if (MH_CreateHook(
                showConfirmTarget,
                reinterpret_cast<void*>(&ShowConfirmMenu),
                reinterpret_cast<void**>(&originalShowConfirmMenu)) !=
            MH_OK) {
            showConfirmTarget = nullptr;
            spdlog::error(
                "Could not create the ExamineMenu::ShowConfirmMenu hook");
            return false;
        }
        if (MH_EnableHook(showConfirmTarget) != MH_OK) {
            MH_RemoveHook(showConfirmTarget);
            showConfirmTarget = nullptr;
            originalShowConfirmMenu = nullptr;
            spdlog::error(
                "Could not enable the ExamineMenu::ShowConfirmMenu hook");
            return false;
        }
        if (!originalShowConfirmMenu) {
            MH_DisableHook(showConfirmTarget);
            MH_RemoveHook(showConfirmTarget);
            showConfirmTarget = nullptr;
            spdlog::error(
                "The ExamineMenu::ShowConfirmMenu trampoline is null");
            return false;
        }
        return true;
    }

    void RemoveShowConfirmHook()
    {
        if (showConfirmTarget) {
            MH_DisableHook(showConfirmTarget);
            MH_RemoveHook(showConfirmTarget);
            showConfirmTarget = nullptr;
            originalShowConfirmMenu = nullptr;
        }
    }
}

bool Hooks::Install()
{
    const MH_STATUS initializeStatus = MH_Initialize();
    if (initializeStatus != MH_OK &&
        initializeStatus != MH_ERROR_ALREADY_INITIALIZED) {
        spdlog::error("Could not initialize MinHook");
        return false;
    }

    if (!InstallShowConfirmHook()) {
        return false;
    }
    if (!InstallUIInputHook()) {
        RemoveShowConfirmHook();
        spdlog::error("Could not install the UI input hook");
        return false;
    }

    spdlog::info(
        "Installed UI input hooks for workshop and workbench "
        "scrapping");
    return true;
}
