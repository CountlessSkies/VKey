// App Overrides Dialog JavaScript
// Handles dropdown selection and C++ communication

var isFullyLoaded = false;

document.ready = function () {
    initSubDialog(".app-list");  // blur + dark + scrollbar + i18n
    initRunningAppsDropdown();
    initButtons();
    initScopeSwitch();
    initEventDelegation();
    requestAnimationFrame(function() {
        requestAnimationFrame(function() {
            isFullyLoaded = true;
            updatePresetButton();  // after i18n ran, so the label follows the UI language
        });
    });
};

// ===== Input Method Labels =====
var inputMethodLabels = {
    "-1": "Mặc định",
    0: "Telex",
    1: "VNI",
    2: "Simple Telex",
    3: "Telex + VNI",
    4: "Tự định nghĩa"
};

var encodingLabels = {
    "-1": "Mặc định",
    0: "Unicode",
    1: "TCVN3",
    2: "VNI Win",
    3: "UNI Cmp",
    4: "CP 1258"
};

var sendMethodLabels = {
    "-1": "Mặc định",
    1: "Clipboard",
    2: "Firefox",
    3: "Cloud/Remote",
    4: "Thay thế trực tiếp",
    5: "Chế độ game",
    6: "WebView App"
};

var sendMethodLabelsEn = {
    "-1": "Default",
    1: "Clipboard",
    2: "Firefox",
    3: "Cloud/Remote",
    4: "Direct replace",
    5: "Game mode",
    6: "WebView App"
};

function getSendMethodLabel(sndKey) {
    var isEn = (typeof getLang === "function" && getLang() === "en");
    var map = isEn ? sendMethodLabelsEn : sendMethodLabels;
    return map[sndKey] || "-";
}

// ===== Running Apps Dropdown =====
function initRunningAppsDropdown() {
    var input = document.getElementById("app-name");
    var dropdown = document.getElementById("running-apps-dropdown");
    var refreshBtn = document.getElementById("btn-refresh");

    if (!input || !dropdown) return;

    // Show dropdown on focus (not while a saved row is being edited: the name is locked)
    input.addEventListener("focus", function () {
        if (editingApp) return;
        triggerAction("get-running-apps");
        setTimeout(function () {
            dropdown.classList.add("visible");
        }, 100);
    });

    // Hide dropdown when clicking outside
    document.addEventListener("click", function (e) {
        if (!input.contains(e.target) && !dropdown.contains(e.target) && !refreshBtn.contains(e.target)) {
            dropdown.classList.remove("visible");
        }
    });

    // Filter on typing
    input.addEventListener("input", function () {
        filterDropdown(input.value.toLowerCase());
    });

    // Refresh button
    if (refreshBtn) {
        refreshBtn.addEventListener("click", function (e) {
            e.stopPropagation();
            triggerAction("get-running-apps");
            input.focus();
        });
    }
}

function filterDropdown(query) {
    var dropdown = document.getElementById("running-apps-dropdown");
    var items = dropdown.querySelectorAll(".dropdown-item");

    items.forEach(function (item) {
        var text = item.textContent.toLowerCase();
        if (text.indexOf(query) >= 0) {
            item.style.display = "block";
        } else {
            item.style.display = "none";
        }
    });
}

// Called from C++ to populate dropdown
function setRunningApps(apps) {
    var dropdown = document.getElementById("running-apps-dropdown");
    if (!dropdown) return;

    dropdown.innerHTML = "";

    if (!apps || apps.length === 0) {
        var empty = document.createElement("div");
        empty.className = "dropdown-empty";
        empty.textContent = (typeof t === "function" && t("no_running_apps")) || "Không có ứng dụng nào đang chạy";
        dropdown.appendChild(empty);
        return;
    }

    apps.forEach(function (app) {
        var item = document.createElement("div");
        item.className = "dropdown-item";
        item.textContent = app;
        item.addEventListener("click", function () {
            document.getElementById("app-name").value = app;
            dropdown.classList.remove("visible");
        });
        dropdown.appendChild(item);
    });

    dropdown.classList.add("visible");
}

// ===== Buttons =====
function initButtons() {
    var addBtn = document.getElementById("btn-add");
    var pickBtn = document.getElementById("btn-pick-window");

    if (addBtn) {
        addBtn.onclick = function () {
            onAddApp();
        };
    }

    if (pickBtn) {
        pickBtn.onclick = function () {
            triggerAction("pick-window");
        };
    }

    var presetBtn = document.getElementById("btn-preset-lol");
    if (presetBtn) {
        presetBtn.onclick = function () {
            // Already in the list: open that row for editing instead.
            var row = findAppItem(LOL_EXE);
            if (row) { startEdit(row); return; }
            triggerAction("add-preset-lol");
        };
    }

    var doneBtn = document.getElementById("btn-edit-done");
    if (doneBtn) {
        doneBtn.onclick = function () { stopEdit(); };
    }
}

// ===== Editing a saved row (variant B: every change saves at once) =====
var editingApp = null;  // lower-case exe name of the row being edited, or null

function findAppItem(appName) {
    var found = null;
    document.querySelectorAll("#app-list .app-item").forEach(function (item) {
        if (item.getAttribute("data-app") === appName) found = item;
    });
    return found;
}

function startEdit(item) {
    var appName = item.getAttribute("data-app");
    if (editingApp === appName) { stopEdit(); return; }
    editingApp = appName;

    document.querySelectorAll("#app-list .app-item").forEach(function (it) {
        if (it === item) it.classList.add("selected");
        else it.classList.remove("selected");
    });

    var input = document.getElementById("app-name");
    if (input) {
        input.value = appName;
        input.setAttribute("readonly", "");
    }
    var dropdown = document.getElementById("running-apps-dropdown");
    if (dropdown) dropdown.classList.remove("visible");

    document.getElementById("input-method").value = item.getAttribute("data-method") || "-1";
    document.getElementById("encoding-override").value = item.getAttribute("data-enc") || "-1";
    var chat = item.getAttribute("data-chat") === "1";
    setScope(chat);
    if (!chat) document.getElementById("send-method").value = item.getAttribute("data-send") || "-1";
    setChatAlwaysVi(chat && item.getAttribute("data-alwaysvi") === "1");

    var card = document.querySelector(".input-card");
    if (card) card.classList.add("editing");
}

function stopEdit() {
    if (!editingApp) return;
    editingApp = null;
    document.querySelectorAll("#app-list .app-item").forEach(function (it) {
        it.classList.remove("selected");
    });
    var input = document.getElementById("app-name");
    if (input) input.removeAttribute("readonly");
    var card = document.querySelector(".input-card");
    if (card) card.classList.remove("editing");
    clearInput();
}

// Called after any form control changes. Outside edit mode nothing is saved
// until "+ Thêm"; while editing, the row is written straight away.
function onFormChanged() {
    if (!editingApp) return;
    fillHiddenInputs(editingApp);
    triggerAction("update-app");
}

// ===== "Tiếng Việt" scope: always / only inside the game's text boxes =====
var LOL_EXE = "league of legends.exe";
var chatOnlyScope = false;
var chatAlwaysVi = false;

function initScopeSwitch() {
    document.on("click", "#vi-scope .scope-opt", function (evt, opt) {
        var chat = opt.getAttribute("data-scope") === "1";
        if (chat === chatOnlyScope) return true;
        setScope(chat);
        if (!chat) setChatAlwaysVi(false);
        onFormChanged();
        return true;
    });
    document.on("click", "#toggle-chat-always-vi", function () {
        if (!chatOnlyScope) return true;
        setChatAlwaysVi(!chatAlwaysVi);
        onFormChanged();
        return true;
    });
    document.on("change", "#input-method, #encoding-override, #send-method", function () {
        onFormChanged();
        return true;
    });
    setScope(false);
}

function setChatAlwaysVi(on) {
    chatAlwaysVi = on;
    var t = document.getElementById("toggle-chat-always-vi");
    if (t) {
        if (on) t.classList.add("checked");
        else t.classList.remove("checked");
    }
}

function scopeLabel(chatOnly, alwaysVi) {
    var isEn = (typeof getLang === "function" && getLang() === "en");
    if (!chatOnly) return isEn ? "Always" : "Luôn bật";
    if (alwaysVi) return isEn ? "Chat · always V" : "Ô chữ · luôn V";
    return isEn ? "Chat only" : "Chỉ ô chữ";
}

function setScope(chatOnly) {
    chatOnlyScope = chatOnly;
    document.querySelectorAll("#vi-scope .scope-opt").forEach(function (opt) {
        if ((opt.getAttribute("data-scope") === "1") === chatOnly) opt.classList.add("active");
        else opt.classList.remove("active");
    });
    var hint = document.getElementById("vi-scope-hint");
    if (hint) hint.style.display = chatOnly ? "block" : "none";
    // The sub-option only means something inside the game's text boxes.
    var viRow = document.getElementById("row-chat-always-vi");
    if (viRow) {
        if (chatOnly) viRow.classList.remove("disabled");
        else viRow.classList.add("disabled");
    }
    // Outside the text boxes VKey already passes every key, so the old
    // "game mode" send method has nothing left to do: lock it to default.
    var send = document.getElementById("send-method");
    var row = document.getElementById("row-send-method");
    if (send) {
        if (chatOnly) send.value = "-1";
        send.state.disabled = chatOnly;
    }
    if (row) {
        if (chatOnly) row.classList.add("disabled");
        else row.classList.remove("disabled");
    }
}

function updatePresetButton() {
    var btn = document.getElementById("btn-preset-lol");
    if (!btn) return;
    var exists = false;
    document.querySelectorAll("#app-list .app-item").forEach(function (item) {
        if (item.getAttribute("data-app") === LOL_EXE) exists = true;
    });
    var isEn = (typeof getLang === "function" && getLang() === "en");
    // Already in the list: the button opens the LoL row for editing.
    btn.classList.remove("done");
    if (exists) {
        btn.textContent = isEn ? "Edit" : "Sửa";
    } else {
        btn.textContent = isEn ? "+ Add" : "+ Thêm";
    }
}

function fillHiddenInputs(appName) {
    document.getElementById("val-app-name").value = appName;
    document.getElementById("val-input-method").value = document.getElementById("input-method").value;
    document.getElementById("val-encoding-override").value = document.getElementById("encoding-override").value;
    document.getElementById("val-send-method").value =
        chatOnlyScope ? "-1" : document.getElementById("send-method").value;
    document.getElementById("val-chat-only").value = chatOnlyScope ? "1" : "0";
    document.getElementById("val-chat-always-vi").value = (chatOnlyScope && chatAlwaysVi) ? "1" : "0";
}

function onAddApp() {
    var appName = document.getElementById("app-name").value.trim();
    if (!appName) {
        return;
    }
    fillHiddenInputs(appName);
    triggerAction("add-app");
}

// ===== Event Delegation for Dynamic Elements =====
function initEventDelegation() {
    // Handle delete button click using event delegation
    document.on("click", ".app-item-delete", function (evt, btn) {
        var item = btn.closest(".app-item");
        if (item) {
            var appName = item.getAttribute("data-app");
            if (appName) {
                if (appName === editingApp) stopEdit();
                document.getElementById("val-app-name").value = appName;
                triggerAction("delete-app");
            }
        }
        evt.stopPropagation();
        return true;
    });

    // Click anywhere else on a row: load its saved settings into the form.
    document.on("click", "#app-list .app-item", function (evt, item) {
        if (evt.target && evt.target.closest(".app-item-delete")) return true;
        startEdit(item);
        return true;
    });
}

// ===== Action Trigger =====
function triggerAction(action) {
    var el = document.getElementById("val-action");
    if (el) {
        el.value = action;
        el.dispatchEvent(new Event("change", { bubbles: true }));
    }
}

// ===== App List Functions (called from C++) =====
function clearAppList() {
    var list = document.getElementById("app-list");
    if (list) list.innerHTML = "";
    updatePresetButton();
}

function addAppToList(appName, inputMethod, encodingOverride, sendMethod, chatOnly) {
    var list = document.getElementById("app-list");
    if (!list) return;

    var item = document.createElement("div");
    item.className = "app-item";
    item.setAttribute("data-app", appName);
    // Saved values, read back by startEdit(). Chat scope is set by setAppScope().
    item.setAttribute("data-method", String(inputMethod === undefined || inputMethod === null ? -1 : inputMethod));
    item.setAttribute("data-enc", String(encodingOverride === undefined || encodingOverride === null ? -1 : encodingOverride));
    item.setAttribute("data-send", String(sendMethod === undefined || sendMethod === null ? -1 : sendMethod));
    item.setAttribute("data-chat", chatOnly ? "1" : "0");
    item.setAttribute("data-alwaysvi", "0");

    // App name column
    var nameSpan = document.createElement("span");
    nameSpan.className = "app-item-name";
    nameSpan.textContent = appName;
    nameSpan.title = appName;
    item.appendChild(nameSpan);

    // Input method column
    var methodSpan = document.createElement("span");
    methodSpan.className = "app-item-inputmethod";
    var mthKey = (inputMethod === undefined || inputMethod === null) ? "-1" : String(inputMethod);
    methodSpan.textContent = inputMethodLabels[mthKey] || "-";
    item.appendChild(methodSpan);

    // Encoding column
    var encodingSpan = document.createElement("span");
    encodingSpan.className = "app-item-encoding";
    var encKey = (encodingOverride === undefined || encodingOverride === null) ? "-1" : String(encodingOverride);
    encodingSpan.textContent = encodingLabels[encKey] || "-";
    item.appendChild(encodingSpan);

    // SendMethod column
    var sendSpan = document.createElement("span");
    sendSpan.className = "app-item-sendmethod";
    var sndKey = (sendMethod === undefined || sendMethod === null) ? "-1" : String(sendMethod);
    sendSpan.textContent = chatOnly ? "—" : getSendMethodLabel(sndKey);
    item.appendChild(sendSpan);

    // Vietnamese scope column
    var scopeSpan = document.createElement("span");
    scopeSpan.className = chatOnly ? "app-item-scope chat" : "app-item-scope";
    scopeSpan.textContent = scopeLabel(!!chatOnly, false);
    item.appendChild(scopeSpan);

    // Delete button (uses event delegation, no inline handler)
    var deleteBtn = document.createElement("button");
    deleteBtn.className = "app-item-delete";
    deleteBtn.textContent = "×";
    item.appendChild(deleteBtn);

    list.appendChild(item);
    updatePresetButton();

    if (isFullyLoaded && typeof showToastI18n === "function") {
        showToastI18n("Đã thêm cấu hình cho: " + appName, "Added override for: " + appName);
    }
}

// Called from C++ right after addAppToList for entries with chat_only = true
// (Sciter's call_function takes at most four arguments).
function setAppChatOnly(appName, alwaysVi) {
    setAppScope(appName, true, alwaysVi);
}

function setAppScope(appName, chatOnly, alwaysVi) {
    var item = findAppItem(appName);
    if (!item) return;
    item.setAttribute("data-chat", chatOnly ? "1" : "0");
    item.setAttribute("data-alwaysvi", (chatOnly && alwaysVi) ? "1" : "0");
    var send = item.querySelector(".app-item-sendmethod");
    if (send) send.textContent = chatOnly ? "—" : getSendMethodLabel(item.getAttribute("data-send") || "-1");
    var scope = item.querySelector(".app-item-scope");
    if (scope) {
        scope.className = chatOnly ? "app-item-scope chat" : "app-item-scope";
        scope.textContent = scopeLabel(!!chatOnly, !!alwaysVi);
    }
}

// Called from C++ after an edited row was saved (update-app): refresh the
// row in place, keep its position and the selection, no toast.
function updateAppInList(appName, inputMethod, encodingOverride, sendMethod) {
    var item = findAppItem(appName);
    if (!item) return;
    item.setAttribute("data-method", String(inputMethod));
    item.setAttribute("data-enc", String(encodingOverride));
    item.setAttribute("data-send", String(sendMethod));
    var m = item.querySelector(".app-item-inputmethod");
    if (m) m.textContent = inputMethodLabels[String(inputMethod)] || "-";
    var e = item.querySelector(".app-item-encoding");
    if (e) e.textContent = encodingLabels[String(encodingOverride)] || "-";
}

function removeAppFromList(appName) {
    var list = document.getElementById("app-list");
    if (!list) return;

    var items = list.querySelectorAll(".app-item");
    items.forEach(function (item) {
        if (item.getAttribute("data-app") === appName) {
            item.remove();
        }
    });
    updatePresetButton();

    if (isFullyLoaded && typeof showToastI18n === "function") {
        showToastI18n("Đã xóa cấu hình của: " + appName, "Removed override for: " + appName);
    }
}

function clearInput() {
    var input = document.getElementById("app-name");
    if (input) input.value = "";

    var methodSelect = document.getElementById("input-method");
    if (methodSelect) methodSelect.value = "-1";

    var encodingSelect = document.getElementById("encoding-override");
    if (encodingSelect) encodingSelect.value = "-1";

    var sendSelect = document.getElementById("send-method");
    if (sendSelect) sendSelect.value = "-1";

    setScope(false);
    setChatAlwaysVi(false);
}

function forceRefresh(scrollToBottom) {
    var list = document.getElementById("app-list");
    if (list && scrollToBottom) {
        list.scrollTop = list.scrollHeight;
    }
}
