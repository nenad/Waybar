#include "modules/workspaces.hpp"
#include "ipc/client.hpp"

waybar::modules::WorkspaceSelector::WorkspaceSelector(Bar &bar)
  : _bar(bar), _box(Gtk::manage(new Gtk::Box))
{
  _box->get_style_context()->add_class("workspace-selector");
  std::string socketPath = get_socketpath();
  _ipcSocketfd = ipc_open_socket(socketPath);
  _ipcEventSocketfd = ipc_open_socket(socketPath);
  const char *subscribe = "[ \"workspace\", \"mode\" ]";
  uint32_t len = strlen(subscribe);
  ipc_single_command(_ipcEventSocketfd, IPC_SUBSCRIBE, subscribe, &len);
  _thread = [this] {
    update();
  };
}

auto waybar::modules::WorkspaceSelector::update() -> void
{
  Json::Value workspaces = _getWorkspaces();
  for (auto it = _buttons.begin(); it != _buttons.end(); ++it) {
    auto ws = std::find_if(workspaces.begin(), workspaces.end(),
      [it](auto node) -> bool { return node["num"].asInt() == it->first; });
    if (ws == workspaces.end()) {
      it->second.hide();
    }
  }
  for (auto node : workspaces) {
    auto it = _buttons.find(node["num"].asInt());
    if (it == _buttons.end()) {
      _addWorkspace(node);
    } else {
      auto styleContext = it->second.get_style_context();
      bool isCurrent = node["focused"].asBool();
      if (styleContext->has_class("current") && !isCurrent) {
        styleContext->remove_class("current");
      } else if (!styleContext->has_class("current") && isCurrent) {
        styleContext->add_class("current");
      }
      it->second.show();
    }
  }
}

void waybar::modules::WorkspaceSelector::_addWorkspace(Json::Value node)
{
  auto pair = _buttons.emplace(node["num"].asInt(), node["name"].asString());
  auto &button = pair.first->second;
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, pair] {
    auto value = fmt::format("workspace \"{}\"", pair.first->first);
    uint32_t size = value.size();
    ipc_single_command(_ipcSocketfd, IPC_COMMAND, value.c_str(), &size);
  });
  _box->pack_start(button, false, false, 0);
  if (node["focused"].asBool()) {
    button.get_style_context()->add_class("current");
  }
  button.show();
}

Json::Value waybar::modules::WorkspaceSelector::_getWorkspaces()
{
  uint32_t len = 0;
  Json::Value root;
  Json::CharReaderBuilder builder;
  Json::CharReader* reader = builder.newCharReader();
  std::string err;
  std::string str = ipc_single_command(_ipcSocketfd, IPC_GET_WORKSPACES,
    nullptr, &len);
  bool res = reader->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
  delete reader;
  if (!res) {
    std::cerr << err << std::endl;
    return nullptr;
  }
  return root;
}

waybar::modules::WorkspaceSelector::operator Gtk::Widget &() {
  return *_box;
}
