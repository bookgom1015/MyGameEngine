#pragma once

#include <vector>

#include "Widget.h"

namespace UI {
	class Layer {
	public:
		Layer() = default;
		virtual ~Layer() = default;

	public:

	private:
		Layer* mChildLayer = nullptr;

		std::vector<Widget> mWidgets;
	};
}