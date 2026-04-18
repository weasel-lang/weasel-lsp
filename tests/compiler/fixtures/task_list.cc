#include "weasel/factory.hpp"
#include "weasel/node.hpp"
#include "weasel/renderer.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

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
    const task&          item;
    std::optional<bool>  show_assignee;
};

struct task_list_props {
    const std::vector<task>& items;
    std::optional<std::string> title;
};

weasel::node badge(const badge_props& p) {
    std::string cls = "badge";
    if (p.color) {
        cls += " badge-" + *p.color;
    }
    return weasel::tag("span", {{"class", (cls)}}, {weasel::text(p.label)});
}

weasel::node task_item(const task_item_props& p) {
    const bool show = p.show_assignee.value_or(true);
    return (
        weasel::tag("li", {{"class", "task-item"}}, {weasel::tag("h3", {{"class", "task-title"}}, {weasel::text(p.item.title)}), [&]() -> weasel::node { if (show && !p.item.assignee.empty()) { return weasel::tag("p", {{"class", "assignee"}}, {weasel::text("Assigned to: "), badge({.label = (p.item.assignee), .color = "blue"})}); } return weasel::node{}; }()})









    );
}

weasel::node task_list(const task_list_props& p) {
    return (
        weasel::tag("div", {{"class", "page"}}, {[&]() -> weasel::node { if (p.title) { return weasel::tag("h2", {}, {weasel::text(*p.title)}); } return weasel::node{}; }(), weasel::tag("ul", {{"class", "task-list"}}, {[&]() -> weasel::node { weasel::node_list __w; for (const auto& t : p.items) { __w.push_back(task_item({.item = (t)})); } return weasel::fragment(std::move(__w)); }()})})













    );
}

int main() {
    const std::vector<task> tasks = {
        {"Fix login bug",       "Alice"},
        {"Write release notes", "Bob"},
        {"Deploy to staging",   ""},
    };

    node page = task_list({.items = tasks, .title = "Sprint Tasks"});

    std::cout << "<!DOCTYPE html>\n"
              << "<html><body>\n";
    render(page, std::cout);
    std::cout << "\n</body></html>\n";
    return 0;
}
