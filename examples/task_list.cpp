#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include "weasel/factory.hpp"
#include "weasel/node.hpp"
#include "weasel/renderer.hpp"

using namespace weasel;

struct task {
    std::string title;
    std::string assignee;
};

struct badge_props {
    std::string label;
    std::optional<std::string> color;
};

struct task_item_props {
    const task& item;
    std::optional<bool> show_assignee;
};

struct task_list_props {
    const std::vector<task>& items;
    std::optional<std::string> title;
};

node badge(const badge_props& p) {
    std::string cls = "badge";
    if (p.color)
        cls += " badge-" + *p.color;
    return tag("span", {{"class", cls}}, {text(p.label)});
}

node task_item(const task_item_props& p) {
    const bool show = p.show_assignee.value_or(true);

    node_list children;
    children.push_back(tag("h3", {{"class", "task-title"}}, {text(p.item.title)}));
    if (show && !p.item.assignee.empty()) {
        children.push_back(tag("p", {{"class", "assignee"}}, {text("Assigned to: "), badge({.label = p.item.assignee, .color = "blue"})}));
    }
    return tag("li", {{"class", "task-item"}}, std::move(children));
}

node task_list(const task_list_props& p) {
    node_list cards;
    for (const auto& t : p.items) {
        cards.push_back(task_item({.item = t}));
    }

    node_list body;
    if (p.title) {
        body.push_back(tag("h2", {}, {text(*p.title)}));
    }
    body.push_back(tag("ul", {{"class", "task-list"}}, std::move(cards)));

    return tag("div", {{"class", "page"}}, std::move(body));
}

int main() {
    const std::vector<task> tasks = {
        {"Fix login bug", "Alice"},
        {"Write release notes", "Bob"},
        {"Deploy to staging", ""},
    };

    node page = task_list({.items = tasks, .title = "Sprint Tasks"});

    std::cout << "<!DOCTYPE html>\n"
              << "<html><body>\n";
    render(page, std::cout);
    std::cout << "\n</body></html>\n";
    return 0;
}
