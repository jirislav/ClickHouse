#include <Storages/ConstraintsDescription.h>

#include <Interpreters/ExpressionAnalyzer.h>
#include <Interpreters/TreeRewriter.h>

#include <Parsers/formatAST.h>
#include <Parsers/ParserCreateQuery.h>
#include <Parsers/parseQuery.h>
#include <Parsers/ASTExpressionList.h>

#include <Core/Defines.h>


namespace DB
{

String ConstraintsDescription::toString() const
{
    if (constraints.empty())
        return {};

    ASTExpressionList list;
    for (const auto & constraint : constraints)
        list.children.push_back(constraint);

    return serializeAST(list, true);
}

ConstraintsDescription ConstraintsDescription::parse(const String & str)
{
    if (str.empty())
        return {};

    ConstraintsDescription res;
    ParserConstraintDeclarationList parser;
    ASTPtr list = parseQuery(parser, str, 0, DBMS_DEFAULT_MAX_PARSER_DEPTH);

    for (const auto & constraint : list->children)
        res.constraints.push_back(constraint);

    return res;
}

ASTs ConstraintsDescription::filterConstraints(ConstraintType type) const
{
    auto constraint_type = (type == ConstraintType::CHECK ? ASTConstraintDeclaration::Type::CHECK : ASTConstraintDeclaration::Type::ASSUME);
    ASTs res;
    res.reserve(constraints.size());
    for (const auto & constraint : constraints)
    {
        if (constraint->as<ASTConstraintDeclaration>()->type == constraint_type) {
            res.push_back(constraint);
        }
    }
    return res;
}

ConstraintsExpressions ConstraintsDescription::getExpressionsToCheck(const DB::Context & context,
                                                              const DB::NamesAndTypesList & source_columns_) const
{
    ConstraintsExpressions res;
    res.reserve(constraints.size());
    for (const auto & constraint : constraints)
    {
        auto * constraint_ptr = constraint->as<ASTConstraintDeclaration>();
        if (constraint_ptr->type == ASTConstraintDeclaration::Type::CHECK)
        {
            // TreeRewriter::analyze has query as non-const argument so to avoid accidental query changes we clone it
            ASTPtr expr = constraint_ptr->expr->clone();
            auto syntax_result = TreeRewriter(context).analyze(expr, source_columns_);
            res.push_back(ExpressionAnalyzer(constraint_ptr->expr->clone(), syntax_result, context).getActions(false));
        }
    }
    return res;
}

ConstraintsDescription::ConstraintsDescription(const ConstraintsDescription & other)
{
    constraints.reserve(other.constraints.size());
    for (const auto & constraint : other.constraints)
        constraints.emplace_back(constraint->clone());
}

ConstraintsDescription & ConstraintsDescription::operator=(const ConstraintsDescription & other)
{
    constraints.resize(other.constraints.size());
    for (size_t i = 0; i < constraints.size(); ++i)
        constraints[i] = other.constraints[i]->clone();
    return *this;
}

}
