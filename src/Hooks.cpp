#include "PCH.h"

#include "Hooks.h"
#include "Settings.h"

namespace
{
    constexpr std::size_t kPerformInputProcessingIndex{ 0 };
    constexpr std::size_t kScrapItemAcceptIndex{ 1 };
    constexpr std::size_t kConfirmAdvanceMovieIndex{ 4 };
    constexpr std::string_view kScrapButtonEvent{ "XButton" };
    constexpr std::string_view kActivateEvent{ "Activate" };

    using ScrapRewards = RE::BSTArray<
        RE::BSTTuple<RE::TESBoundObject*, std::uint32_t>>;
    using WorkshopReference = RE::BSPointerHandleSmartPointer<
        RE::BSPointerHandleManagerInterface<
            RE::TESObjectREFR,
            RE::HandleManager>>;

    struct Reward
    {
        std::string name;
        std::uint64_t count{ 0 };
    };

    using InputProcessor = void (*)(
        RE::UI*,
        const RE::InputEvent*);
    using ShowConfirmHandler = void (*)(
        RE::ExamineMenu*,
        RE::ExamineConfirmMenu::InitData*,
        RE::ExamineConfirmMenu::ICallback*);
    using ScrapItemAcceptHandler = void (*)(RE::ScrapItemCallback*);
    using ConfirmAdvanceMovieHandler = void (*)(
        RE::ExamineConfirmMenu*,
        float,
        std::uint64_t);
    using ScrapReferenceHandler = void (*)(
        const RE::Workshop::ContextData&,
        WorkshopReference&,
        ScrapRewards*);
    using ShowHUDMessageHandler = void (*)(
        const char*,
        const char*,
        bool,
        bool);

    InputProcessor originalInputProcessor{ nullptr };
    ShowConfirmHandler originalShowConfirmMenu{ nullptr };
    ScrapItemAcceptHandler originalScrapItemAccept{ nullptr };
    ConfirmAdvanceMovieHandler originalConfirmAdvanceMovie{ nullptr };
    ScrapReferenceHandler originalScrapReference{ nullptr };
    void* showConfirmTarget{ nullptr };
    void* scrapReferenceTarget{ nullptr };
    std::atomic<bool> workshopScrapButtonReleased{ true };
    std::atomic<bool> workshopConfirmOnRelease{ false };
    std::atomic<bool> workshopScrapPending{ false };
    std::atomic<bool> workbenchScrapButtonReleased{ true };
    std::atomic<bool> workbenchConfirmOnRelease{ false };
    std::atomic<bool> workbenchScrapConfirmation{ false };
    std::atomic<bool> instantAcceptPending{ false };
    std::vector<Reward> pendingWorkbenchRewards;

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

    bool IsInstantMode()
    {
        return Settings::Get().mode == Settings::ScrapMode::kInstant;
    }

    void ResetScrapState()
    {
        workshopScrapButtonReleased.store(true, std::memory_order_release);
        workshopConfirmOnRelease.store(false, std::memory_order_release);
        workshopScrapPending.store(false, std::memory_order_release);
        workbenchScrapButtonReleased.store(true, std::memory_order_release);
        workbenchConfirmOnRelease.store(false, std::memory_order_release);
        workbenchScrapConfirmation.store(false, std::memory_order_release);
        instantAcceptPending.store(false, std::memory_order_release);
    }

    void InitializeConfirmEvent(RE::ButtonEvent& event)
    {
        REX::EMPLACE_VTABLE(&event);
        event.device = RE::INPUT_DEVICE::kKeyboard;
        event.eventType = RE::INPUT_EVENT_TYPE::kButton;
        event.strUserEvent = kActivateEvent;
        if (const auto* controlMap = GetControlMap()) {
            event.idCode = static_cast<std::int32_t>(
                controlMap->GetMappedKey(
                    kActivateEvent,
                    RE::INPUT_DEVICE::kKeyboard,
                    RE::UserEvents::INPUT_CONTEXT_ID::kMainGameplay));
        }
        event.disabled = true;
        event.handled = RE::InputEvent::HANDLED_RESULT::kContinue;
        event.value = 0.0F;
        event.heldDownSecs = 0.1F;
    }

    void ConfirmAdvanceMovie(
        RE::ExamineConfirmMenu* menu,
        const float timeDelta,
        const std::uint64_t time)
    {
        const bool instantPending =
            IsInstantMode() &&
            instantAcceptPending.load(std::memory_order_acquire);
        if (instantPending) {
            menu->menuCanBeVisible = false;
        }

        originalConfirmAdvanceMovie(menu, timeDelta, time);

        if (!instantPending) {
            return;
        }

        instantAcceptPending.store(false, std::memory_order_release);
        RE::ButtonEvent confirmEvent{};
        InitializeConfirmEvent(confirmEvent);
        if (auto* ui = RE::UI::GetSingleton()) {
            originalInputProcessor(ui, &confirmEvent);
        }
        ResetScrapState();
        spdlog::debug("Accepted hidden instant scrap confirmation");
    }

    std::vector<Reward> CopyRewards(const ScrapRewards& rewards)
    {
        std::vector<Reward> result;
        for (const auto& [object, count] : rewards) {
            if (!object || count == 0) {
                continue;
            }

            auto name = RE::TESFullName::GetFullName(*object);
            std::string ownedName{ name };
            if (ownedName.empty()) {
                ownedName = std::format(
                    "Form {:08X}",
                    object->GetFormID());
            }

            const auto existing = std::ranges::find(
                result,
                ownedName,
                &Reward::name);
            if (existing != result.end()) {
                existing->count += count;
            } else {
                result.push_back({ std::move(ownedName), count });
            }
        }
        return result;
    }

    void ShowMaterialSummary(const std::vector<Reward>& rewards)
    {
        if (!Settings::Get().showMaterials || rewards.empty()) {
            return;
        }

        std::string message{ "Recovered: " };
        for (std::size_t index = 0; index < rewards.size(); ++index) {
            if (index > 0) {
                message.append(", ");
            }
            message.append(std::format(
                "{} ({})",
                rewards[index].name,
                rewards[index].count));
        }

        static REL::Relocation<ShowHUDMessageHandler> showHUDMessage{
            RuntimeID(1163005, 2222440, 2222440)
        };
        showHUDMessage(message.c_str(), nullptr, false, false);
        spdlog::debug("{}", message);
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

    void ProcessAsConfirm(
        RE::UI* self,
        RE::ButtonEvent* event,
        const RE::InputEvent* queueHead)
    {
        const RE::BSFixedString originalUserEvent{ event->strUserEvent };
        const auto originalIDCode = event->idCode;
        const auto originalHandled = event->handled;
        const bool originalDisabled = event->disabled;

        event->strUserEvent = kActivateEvent;
        if (const auto* controlMap = GetControlMap()) {
            event->idCode = static_cast<std::int32_t>(
                controlMap->GetMappedKey(
                    kActivateEvent,
                    event->device.get(),
                    RE::UserEvents::INPUT_CONTEXT_ID::kMainGameplay));
        }
        event->disabled = true;
        event->handled = RE::InputEvent::HANDLED_RESULT::kContinue;
        originalInputProcessor(self, queueHead);

        event->strUserEvent = originalUserEvent;
        event->idCode = originalIDCode;
        event->disabled = originalDisabled;
        event->handled = originalHandled;
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
        const bool instantMode = IsInstantMode();
        RE::ButtonEvent* confirmEvent{ nullptr };
        bool markInstantAccept{ false };

        for (auto* input = queueHead; input; input = input->next) {
            const auto* button = input->As<RE::ButtonEvent>();
            if (!IsScrapButtonEvent(button)) {
                continue;
            }

            bool forwardAsConfirm{ false };
            if (workshopConfirmation) {
                if (instantMode) {
                    continue;
                }
                if (button->QReleased()) {
                    forwardAsConfirm =
                        workshopConfirmOnRelease.exchange(
                            false,
                            std::memory_order_acq_rel);
                    if (!forwardAsConfirm) {
                        workshopScrapButtonReleased.store(
                            true,
                            std::memory_order_release);
                    }
                } else if (button->QJustPressed()) {
                    if (workshopScrapButtonReleased.exchange(
                            false,
                            std::memory_order_acq_rel)) {
                        workshopConfirmOnRelease.store(
                            true,
                            std::memory_order_release);
                    }
                }
            } else if (workshopOpen) {
                if (button->QJustPressed()) {
                    workshopScrapButtonReleased.store(
                        false,
                        std::memory_order_release);
                    workshopScrapPending.store(
                        true,
                        std::memory_order_release);
                    markInstantAccept = instantMode;
                } else if (button->QReleased()) {
                    workshopScrapButtonReleased.store(
                        true,
                        std::memory_order_release);
                }
            }

            if (workbenchConfirmation) {
                if (instantMode) {
                    continue;
                }
                if (button->QReleased()) {
                    const bool workbenchForward =
                        workbenchConfirmOnRelease.exchange(
                            false,
                            std::memory_order_acq_rel);
                    forwardAsConfirm =
                        forwardAsConfirm || workbenchForward;
                    if (!workbenchForward) {
                        workbenchScrapButtonReleased.store(
                            true,
                            std::memory_order_release);
                    }
                } else if (button->QJustPressed()) {
                    if (workbenchScrapButtonReleased.exchange(
                            false,
                            std::memory_order_acq_rel)) {
                        workbenchConfirmOnRelease.store(
                            true,
                            std::memory_order_release);
                    }
                }
            } else if (workbenchOpen) {
                if (button->QJustPressed()) {
                    workbenchScrapButtonReleased.store(
                        false,
                        std::memory_order_release);
                    markInstantAccept = instantMode;
                } else if (button->QReleased()) {
                    workbenchScrapButtonReleased.store(
                        true,
                        std::memory_order_release);
                }
            }

            if (forwardAsConfirm && !confirmEvent) {
                confirmEvent = const_cast<RE::ButtonEvent*>(button);
            }
        }

        if (markInstantAccept) {
            instantAcceptPending.store(true, std::memory_order_release);
        }

        if (confirmEvent) {
            ProcessAsConfirm(self, confirmEvent, queueHead);
            ResetScrapState();
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
        pendingWorkbenchRewards.clear();
        if (isScrap && Settings::Get().showMaterials) {
            const auto* scrapData = static_cast<
                RE::ExamineConfirmMenu::InitDataScrap*>(data);
            pendingWorkbenchRewards = CopyRewards(scrapData->scrapResults);
        }

        originalShowConfirmMenu(menu, data, callback);
    }

    void ScrapItemAccept(RE::ScrapItemCallback* callback)
    {
        auto rewards = std::exchange(
            pendingWorkbenchRewards,
            std::vector<Reward>{});
        originalScrapItemAccept(callback);
        ShowMaterialSummary(rewards);
    }

    void ScrapReference(
        const RE::Workshop::ContextData& context,
        WorkshopReference& scrapReference,
        ScrapRewards* rewards)
    {
        originalScrapReference(context, scrapReference, rewards);
        if (rewards && Settings::Get().showMaterials) {
            ShowMaterialSummary(CopyRewards(*rewards));
        }
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

    bool InstallConfirmAdvanceMovieHook()
    {
        REL::Relocation<std::uintptr_t> confirmMenuVtable{
            RE::VTABLE::ExamineConfirmMenu[0]
        };
        originalConfirmAdvanceMovie =
            reinterpret_cast<ConfirmAdvanceMovieHandler>(
                confirmMenuVtable.write_vfunc(
                    kConfirmAdvanceMovieIndex,
                    &ConfirmAdvanceMovie));
        return originalConfirmAdvanceMovie != nullptr;
    }

    bool InstallScrapItemAcceptHook()
    {
        REL::Relocation<std::uintptr_t> scrapItemVtable{
            RE::VTABLE::__ScrapItemCallback[0]
        };
        originalScrapItemAccept =
            reinterpret_cast<ScrapItemAcceptHandler>(
                scrapItemVtable.write_vfunc(
                    kScrapItemAcceptIndex,
                    &ScrapItemAccept));
        return originalScrapItemAccept != nullptr;
    }

    bool InstallScrapReferenceHook()
    {
        REL::Relocation<std::uintptr_t> target{
            RuntimeID(636327, 2195125, 2195125)
        };
        scrapReferenceTarget = reinterpret_cast<void*>(target.address());
        if (MH_CreateHook(
                scrapReferenceTarget,
                reinterpret_cast<void*>(&ScrapReference),
                reinterpret_cast<void**>(&originalScrapReference)) !=
            MH_OK) {
            scrapReferenceTarget = nullptr;
            spdlog::error(
                "Could not create the Workshop::ScrapReference hook");
            return false;
        }
        if (MH_EnableHook(scrapReferenceTarget) != MH_OK) {
            MH_RemoveHook(scrapReferenceTarget);
            scrapReferenceTarget = nullptr;
            originalScrapReference = nullptr;
            spdlog::error(
                "Could not enable the Workshop::ScrapReference hook");
            return false;
        }
        return originalScrapReference != nullptr;
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

    void RemoveScrapReferenceHook()
    {
        if (scrapReferenceTarget) {
            MH_DisableHook(scrapReferenceTarget);
            MH_RemoveHook(scrapReferenceTarget);
            scrapReferenceTarget = nullptr;
            originalScrapReference = nullptr;
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
    if (!InstallScrapReferenceHook()) {
        RemoveShowConfirmHook();
        return false;
    }
    if (!InstallScrapItemAcceptHook()) {
        RemoveScrapReferenceHook();
        RemoveShowConfirmHook();
        spdlog::error("Could not install the scrap item accept hook");
        return false;
    }
    if (!InstallConfirmAdvanceMovieHook()) {
        RemoveScrapReferenceHook();
        RemoveShowConfirmHook();
        spdlog::error(
            "Could not install the confirmation movie hook");
        return false;
    }
    if (!InstallUIInputHook()) {
        RemoveScrapReferenceHook();
        RemoveShowConfirmHook();
        spdlog::error("Could not install the UI input hook");
        return false;
    }

    spdlog::info(
        "Installed workshop and workbench scrap hooks");
    return true;
}
