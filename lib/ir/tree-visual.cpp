/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/ir/tree-visual.hpp>
#include "ansi.hpp"

namespace blm
{
   TreePrinter::TreePrinter(std::ostream& os) : os(os) {}

   void TreePrinter::print_module(const Module& module)
   {
       os << ANSI_BOLD << ANSI_BLUE << "Module: " << module.get_name() << ANSI_RESET << "\n";
       if (Region* rodata = module.get_rodata_region())
       {
           os << TREE_BRANCH << ANSI_CYAN << ".rodata " << ANSI_DIM << "["
              << rodata->get_nodes().size() << " nodes]" << ANSI_RESET << "\n";
       }
       if (Region* root = module.get_root_region())
       {
           size_t global_count = 0;
           for (Node* node : root->get_nodes())
           {
               if (node->ir_type != NodeType::FUNCTION)
                   global_count++;
           }

           if (global_count > 0)
           {
               os << TREE_BRANCH << ANSI_CYAN << ".global " << ANSI_DIM << "["
                  << global_count << " nodes]" << ANSI_RESET << "\n";
           }
       }

       const auto& functions = module.get_functions();
       if (!functions.empty())
       {
           os << TREE_LAST << ANSI_YELLOW << "functions:" << ANSI_RESET << "\n";
           for (std::size_t i = 0; i < functions.size(); ++i)
               print_function_tree(functions[i], module);
       }
   }

   void TreePrinter::print_function_tree(Node* func, const Module& module)
   {
       if (!func || func->ir_type != NodeType::FUNCTION)
           return;

       const auto func_name = std::string(module.get_context().get_string(func->str_id));

       std::string attrs;
       if ((func->props & NodeProps::DRIVER) != NodeProps::NONE)
           attrs += "[driver]";
       if ((func->props & NodeProps::EXPORT) != NodeProps::NONE)
           attrs += "[export]";
       if ((func->props & NodeProps::EXTERN) != NodeProps::NONE)
           attrs += "[extern]";

       os << TREE_SPACE << TREE_LAST << ANSI_WHITE << func_name << "()"
          << (attrs.empty() ? "" : " ") << ANSI_DIM << attrs << ANSI_RESET << "\n";

       for (const Region* child : module.get_root_region()->get_children())
       {
           if (child->get_name() == func_name)
           {
               print_region(child, 2, true);
               break;
           }
       }
   }

   void TreePrinter::print_region(const Region* region, int depth, bool is_last) // NOLINT(*-no-recursion)
   {
       if (!region)
           return;

       const std::string indent = get_indent(depth, is_last);
       const auto region_name = std::string(region->get_name());
       const std::size_t node_count = region->get_nodes().size();

       std::string status;
       if (has_entry_node(region))
           status = " " + std::string(ANSI_BRIGHT_GREEN) + CHECK_MARK + " ENTRY" + std::string(ANSI_RESET);

       os << indent << ANSI_WHITE << region_name << ": " << ANSI_DIM << "["
          << node_count << " nodes]" << ANSI_RESET << status << "\n";

       const auto& children = region->get_children();
       for (std::size_t i = 0; i < children.size(); ++i)
       {
           const bool is_last_child = (i == children.size() - 1);
           print_region(children[i], depth + 1, is_last_child);
       }
   }

    bool TreePrinter::has_entry_node(const Region* region)
   {
       if (!region)
           return false;

       const auto& nodes = region->get_nodes();
       if (nodes.empty())
           return false;

       /* ENTRY node must be the first node in the region */
       return nodes[0]->ir_type == NodeType::ENTRY;
   }

   std::string TreePrinter::get_indent(int depth, bool is_last)
   {
       std::string result;
       for (int i = 0; i < depth - 1; ++i)
           result += TREE_SPACE;

       if (depth > 0)
           result += is_last ? TREE_LAST : TREE_BRANCH;
       return result;
   }
}
