#include "GuiElements/GuiWindow.h"

namespace SciRenderer
{
  GuiWindow::GuiWindow()
  { }

  GuiWindow::~GuiWindow()
  { }

  void
  GuiWindow::onImGuiRender(bool &isOpen, Shared<Scene> activeScene)
  { }

  void
  GuiWindow::onUpdate(float dt, Shared<Scene> activeScene)
  { }

  void
  GuiWindow::onEvent(Event &event)
  { }
}
