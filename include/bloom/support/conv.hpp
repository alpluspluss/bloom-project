/* this project is part of the bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/foundation/node.hpp>

namespace blm
{
	inline const char* node_type_to_string(const NodeType type)
	{
		switch (type)
		{
			case NodeType::ENTRY: return "ENTRY";
			case NodeType::EXIT: return "EXIT";
			case NodeType::PARAM: return "PARAM";
			case NodeType::LIT: return "LIT";
			case NodeType::ADD: return "ADD";
			case NodeType::SUB: return "SUB";
			case NodeType::MUL: return "MUL";
			case NodeType::DIV: return "DIV";
			case NodeType::GT: return "GT";
			case NodeType::GTE: return "GTE";
			case NodeType::LT: return "LT";
			case NodeType::LTE: return "LTE";
			case NodeType::EQ: return "EQ";
			case NodeType::NEQ: return "NEQ";
			case NodeType::BAND: return "BAND";
			case NodeType::BOR: return "BOR";
			case NodeType::BXOR: return "BXOR";
			case NodeType::BNOT: return "BNOT";
			case NodeType::BSHL: return "BSHL";
			case NodeType::BSHR: return "BSHR";
			case NodeType::RET: return "RET";
			case NodeType::FUNCTION: return "FUNCTION";
			case NodeType::CALL: return "CALL";
			case NodeType::CALL_PARAM: return "CALL_PARAM";
			case NodeType::CALL_RESULT: return "CALL_RESULT";
			case NodeType::STACK_ALLOC: return "STACK_ALLOC";
			case NodeType::HEAP_ALLOC: return "HEAP_ALLOC";
			case NodeType::FREE: return "FREE";
			case NodeType::LOAD: return "LOAD";
			case NodeType::STORE: return "STORE";
			case NodeType::ADDR_OF: return "ADDR_OF";
			case NodeType::PTR_LOAD: return "PTR_LOAD";
			case NodeType::PTR_STORE: return "PTR_STORE";
			case NodeType::PTR_ADD: return "PTR_ADD";
			case NodeType::REINTERPRET_CAST: return "REINTERPRET_CAST";
			case NodeType::ATOMIC_LOAD: return "ATOMIC_LOAD";
			case NodeType::ATOMIC_STORE: return "ATOMIC_STORE";
			case NodeType::MOD: return "MOD";
			case NodeType::JUMP: return "JUMP";
			case NodeType::BRANCH: return "BRANCH";
			case NodeType::INVOKE: return "INVOKE";
			case NodeType::VECTOR_BUILD: return "VECTOR_BUILD";
			case NodeType::VECTOR_SPLAT: return "VECTOR_SPLAT";
			case NodeType::VECTOR_EXTRACT: return "VECTOR_EXTRACT";
			default: return "UNKNOWN";
		}
	}
}
