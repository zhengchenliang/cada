
#include "Compare.hh"
#include "NEvaluator.hh"
#include <cmath>

using namespace cadabra;

// void NEvaluator::find_common_subexpressions(std::vector<Ex *>)
// 	{
// 	// Compute the hash value of every subtree, and collect matches.
// 	// Then compare subtrees with equal hash to find common subtrees.
// 	}

NEvaluator::NEvaluator(const Ex &ex_)
	: ex(ex_)
	{
	}

NTensor NEvaluator::evaluate()
	{
	find_variable_locations();

	// The vector below pairs LaTeX strings which can appear in the
	// cadabra input to function names in the C++ standard library.
	
	const std::vector<std::pair<nset_t::iterator, double (*)(double) >> elementary
		= { { name_set.find("\\sin"), std::sin},
			 { name_set.find("\\cos"), std::cos},
			 { name_set.find("\\tan"), std::tan},

			 { name_set.find("\\arcsin"), std::asin},
			 { name_set.find("\\arccos"), std::acos},
			 { name_set.find("\\arctan"), std::atan},
			 
			 { name_set.find("\\sinh"), std::sinh},
			 { name_set.find("\\cosh"), std::cosh},
			 { name_set.find("\\tanh"), std::tanh},

//			 { name_set.find("\\coth"), std::coth},
//			 { name_set.find("\\sech"), std::sech},
//			 { name_set.find("\\csch"), std::csch},

			 { name_set.find("\\arcsinh"), std::asinh},
			 { name_set.find("\\arccosh"), std::acosh},
			 { name_set.find("\\arctanh"), std::atanh},

//			 { name_set.find("\\arccoth"), std::acoth},
//			 { name_set.find("\\arcsech"), std::asech},
//			 { name_set.find("\\arccsch"), std::acsch},

			 { name_set.find("\\log"),     std::log10},
			 { name_set.find("\\ln"),      std::log},
			 { name_set.find("\\exp"),     std::exp},
			 { name_set.find("\\sqrt"),    std::sqrt},			 			 

	};

	const auto n_pow  = name_set.find("\\pow");
	const auto n_prod = name_set.find("\\prod");
	const auto n_sum  = name_set.find("\\sum");
	const auto n_pi   = name_set.find("\\pi");

	NTensor lastval(0);

	auto it = ex.begin_post();
	while(it != ex.end_post()) {
		// Either this node is known in the subtree value map,
		// or this node is a function which combines the values
		// of child nodes.


		auto fnd = subtree_values.find(Ex::iterator(it));
		if(fnd!=subtree_values.end()) {
			//std::cerr << it << " has value " << fnd->second << std::endl;
			}
		else {
			bool found_elementary=false;
			if(it->is_rational()) {
				lastval = to_double(*it->multiplier);
				found_elementary=true;
				}
			else {
				for(const auto& el: elementary) {
					if(it->name == el.first) {
						auto arg    = ex.begin(it);
						auto argval = subtree_values.find(arg)->second;
						lastval  = argval.apply(el.second);
						lastval *= to_double(*it->multiplier);
						found_elementary=true;
						break;
						}
					}
				}
			
			if(found_elementary==false) {
				if(it->name==n_prod) {
					for(auto cit = ex.begin(it); cit!=ex.end(it); ++cit) {
						auto cfnd = subtree_values.find(Ex::iterator(cit));
						if(cfnd==subtree_values.end())
							throw std::logic_error("Inconsistent value tree.");
						if(cit==ex.begin(it))
							lastval = cfnd->second;
						else
							lastval *= cfnd->second;
						}
					lastval *= to_double(*it->multiplier);
					}
				else if(it->name==n_sum) {
					for(auto cit = ex.begin(it); cit!=ex.end(it); ++cit) {
						auto cfnd = subtree_values.find(Ex::iterator(cit));
						if(cfnd==subtree_values.end())
							throw std::logic_error("Inconsistent value tree.");
						if(cit==ex.begin(it))
							lastval = cfnd->second;
						else
							lastval += cfnd->second;
						}
					lastval *= to_double(*it->multiplier);
					}
				else if(it->name==n_pow) {
					auto cit1  = Ex::begin(it);
					auto cit2  = cit1;
					++cit2;
					
					auto cfnd1 = subtree_values.find(Ex::iterator(cit1));
					auto cfnd2 = subtree_values.find(Ex::iterator(cit2));
					if(cfnd1==subtree_values.end() || cfnd2==subtree_values.end())
						throw std::logic_error("Inconsistent value tree at exponentiation node.");

					lastval = cfnd1->second.pow( cfnd2->second );
					lastval *= to_double(*it->multiplier);
					// throw std::logic_error("Value unknown for subtree special function.");
					}
				else if(it->name==n_pi) {
					lastval  = 3.141592653589793238463;
					lastval *= to_double(*it->multiplier);
					}
				else {
					// Try variable substitution rules.
					bool found=false;
					for(const auto& var: variable_values) {
						// std::cerr << "Comparing " << var.first << " with " << *it << std::endl;
						Ex no_multiplier(it);
						auto mult = *it->multiplier;
						one( no_multiplier.begin()->multiplier );
						if(var.variable == no_multiplier) {
							subtree_values.insert(std::make_pair(it, var.values));
							lastval = var.values;
							lastval *= to_double(mult);
							// std::cerr << "We know the value of " << *it << std::endl;
							found=true;
							break;
							}
						}
					if(!found)
						throw std::logic_error("Value unknown for subtree with head "+(*it->name)+".");
					}
				}
			subtree_values.insert(std::make_pair(it, lastval));
			}

		++it;
		}

	return lastval;
	}

void NEvaluator::set_variable(const Ex& var, const NTensor& val)
	{
	variable_values.push_back( VariableValues({var, val}) );
	}

void NEvaluator::find_variable_locations()
	{
	// FIXME: we don't really need this anymore, as we do everything
	// with broadcasting.
	for(auto& var: variable_values) {
		auto it = ex.begin_post();
		while(it != ex.end_post()) {
			if(var.variable == *it)
				var.locations.push_back(it);
			++it;
			}
		// std::cerr << "Variable " << var.variable << " at " << var.locations.size() << " places" << std::endl;
		}

	// Now insert subtree values which are such that for every
	// variable node we have an NTensor which is broadcast to the
	// shape of the full variable set NTensor.

	// std::cerr << "full shape = ";
	std::vector<size_t> fullshape;
	for(const auto& var: variable_values) {
		assert(var.values.shape.size()==1);
		fullshape.push_back(var.values.shape[0]);
		// std::cerr << var.values.shape[0] << ", ";
		}
	// std::cerr << std::endl;

	for(size_t v=0; v<variable_values.size(); ++v) {
		const auto& var = variable_values[v];
		for(const auto& it: var.locations) {
			subtree_values.insert(std::make_pair(it, var.values.broadcast( fullshape, v ) ) );
			}
		}

	// std::cerr << "ready" << std::endl;
	}
