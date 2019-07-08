//===--- ASTScopeImpl.cpp - Swift Object-Oriented AST Scope ---------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
///
/// This file implements the common functions of the 49 ontology.
///
//===----------------------------------------------------------------------===//
#include "swift/AST/ASTScope.h"

#include "swift/AST/ASTContext.h"
#include "swift/AST/ASTWalker.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Expr.h"
#include "swift/AST/Initializer.h"
#include "swift/AST/LazyResolver.h"
#include "swift/AST/Module.h"
#include "swift/AST/NameLookup.h"
#include "swift/AST/ParameterList.h"
#include "swift/AST/Pattern.h"
#include "swift/AST/Stmt.h"
#include "swift/AST/TypeRepr.h"
#include "swift/Basic/NullablePtr.h"
#include "swift/Basic/STLExtras.h"
#include "llvm/Support/Compiler.h"
#include <algorithm>

using namespace swift;
using namespace ast_scope;

#pragma mark ASTScope

llvm::SmallVector<const ASTScopeImpl *, 0> ASTScope::unqualifiedLookup(
    SourceFile *SF, DeclName name, SourceLoc loc,
    const DeclContext *startingContext,
    namelookup::AbstractASTScopeDeclConsumer &consumer) {
  return ASTScopeImpl::unqualifiedLookup(SF, name, loc, startingContext,
                                         consumer);
}

Optional<bool> ASTScope::computeIsCascadingUse(
    ArrayRef<const ast_scope::ASTScopeImpl *> history,
    Optional<bool> initialIsCascadingUse) {
  return ASTScopeImpl::computeIsCascadingUse(history, initialIsCascadingUse);
}

void ASTScope::dump() const { impl->dump(); }
void ASTScope::print(llvm::raw_ostream &out) const { impl->print(out); }
void ASTScope::dumpOneScopeMapLocation(
    std::pair<unsigned, unsigned> lineCol) const {
  impl->dumpOneScopeMapLocation(lineCol);
}

#pragma mark ASTScopeImpl


const PatternBindingEntry &AbstractPatternEntryScope::getPatternEntry() const {
  return decl->getPatternList()[patternEntryIndex];
}

Pattern *AbstractPatternEntryScope::getPattern() const {
  return getPatternEntry().getPattern();
}

NullablePtr<ClosureExpr> BraceStmtScope::parentClosureIfAny() const {
  return !getParent() ? nullptr : getParent().get()->getClosureIfClosureScope();
}

NullablePtr<ClosureExpr> ASTScopeImpl::getClosureIfClosureScope() const {
  return nullptr;
}
NullablePtr<ClosureExpr>
AbstractClosureScope::getClosureIfClosureScope() const {
  return closureExpr;
}

Decl *ASTScopeImpl::getEnclosingAbstractFunctionOrSubscriptDecl() const {
  return getParent().get()->getEnclosingAbstractFunctionOrSubscriptDecl();
}
Decl *
AbstractFunctionDeclScope::getEnclosingAbstractFunctionOrSubscriptDecl() const {
  return decl;
}
Decl *SubscriptDeclScope::getEnclosingAbstractFunctionOrSubscriptDecl() const {
  return decl;
}

// Conservative, because using precise info would be circular
SourceRange AttachedPropertyWrapperScope::getCustomAttributesSourceRange(
    const VarDecl *const vd) {
  SourceRange sr;
  for (auto *attr : vd->getAttrs().getAttributes<CustomAttr>()) {
    if (sr.isInvalid())
      sr = attr->getTypeLoc().getSourceRange();
    else
      sr.widen(attr->getTypeLoc().getSourceRange());
  }
  return sr;
}

SourceManager &ASTScopeImpl::getSourceManager() const {
  return getASTContext().SourceMgr;
}

Stmt *LabeledConditionalStmtScope::getStmt() const {
  return getLabeledConditionalStmt();
}

bool AbstractFunctionBodyScope::isAMethod(
    const AbstractFunctionDecl *const afd) {
  // What makes this interesting is that a method named "init" which is not
  // in a nominal type or extension decl body still gets an implicit self
  // parameter (even though the program is illegal).
  // So when choosing between creating a MethodBodyScope and a
  // PureFunctionBodyScope do we go by the enclosing Decl (i.e.
  // "afd->getDeclContext()->isTypeContext()") or by
  // "bool(afd->getImplicitSelfDecl())"?
  //
  // Since the code uses \c getImplicitSelfDecl, use that.
  return afd->getImplicitSelfDecl();
}

#pragma mark getLabeledConditionalStmt
LabeledConditionalStmt *IfStmtScope::getLabeledConditionalStmt() const {
  return stmt;
}
LabeledConditionalStmt *WhileStmtScope::getLabeledConditionalStmt() const {
  return stmt;
}
LabeledConditionalStmt *GuardStmtScope::getLabeledConditionalStmt() const {
  return stmt;
}


#pragma mark getASTContext

ASTContext &ASTScopeImpl::getASTContext() const {
  if (auto d = getDecl())
    return d.get()->getASTContext();
  if (auto dc = getDeclContext())
    return dc.get()->getASTContext();
  return getParent().get()->getASTContext();
}

#pragma mark getDeclContext

NullablePtr<DeclContext> ASTScopeImpl::getDeclContext() const {
  return nullptr;
}

NullablePtr<DeclContext> ASTSourceFileScope::getDeclContext() const {
  return NullablePtr<DeclContext>(SF);
}

NullablePtr<DeclContext> GenericTypeOrExtensionScope::getDeclContext() const {
  return getGenericContext();
}

NullablePtr<DeclContext> GenericParamScope::getDeclContext() const {
  return dyn_cast<DeclContext>(holder);
}

NullablePtr<DeclContext> PatternEntryInitializerScope::getDeclContext() const {
  return getPatternEntry().getInitContext();
}

NullablePtr<DeclContext> BraceStmtScope::getDeclContext() const {
  return getParent().get()->getDeclContext();
}

NullablePtr<DeclContext>
DefaultArgumentInitializerScope::getDeclContext() const {
  auto *dc = decl->getDefaultArgumentInitContext();
  assert(dc && "If scope exists, this must exist");
  return dc;
}

// When asked for a loc in an intializer in a capture list, the asked-for
// context is the closure.
NullablePtr<DeclContext> CaptureListScope::getDeclContext() const {
  return expr->getClosureBody();
}

NullablePtr<DeclContext> AttachedPropertyWrapperScope::getDeclContext() const {
  return decl->getParentPatternBinding()
      ->getPatternList()
      .front()
      .getInitContext();
}

NullablePtr<DeclContext> AbstractFunctionDeclScope::getDeclContext() const {
  return decl;
}

NullablePtr<DeclContext> AbstractFunctionParamsScope::getDeclContext() const {
  return matchingContext;
}

#pragma mark getClassName

std::string GenericTypeOrExtensionScope::getClassName() const {
  return declKindName() + portionName() + "Scope";
}

#define DEFINE_GET_CLASS_NAME(Name)                                            \
  std::string Name::getClassName() const { return #Name; }

DEFINE_GET_CLASS_NAME(ASTSourceFileScope)
DEFINE_GET_CLASS_NAME(GenericParamScope)
DEFINE_GET_CLASS_NAME(AbstractFunctionDeclScope)
DEFINE_GET_CLASS_NAME(AbstractFunctionParamsScope)
DEFINE_GET_CLASS_NAME(MethodBodyScope)
DEFINE_GET_CLASS_NAME(PureFunctionBodyScope)
DEFINE_GET_CLASS_NAME(DefaultArgumentInitializerScope)
DEFINE_GET_CLASS_NAME(AttachedPropertyWrapperScope)
DEFINE_GET_CLASS_NAME(PatternEntryDeclScope)
DEFINE_GET_CLASS_NAME(PatternEntryInitializerScope)
DEFINE_GET_CLASS_NAME(PatternEntryUseScope)
DEFINE_GET_CLASS_NAME(ConditionalClauseScope)
DEFINE_GET_CLASS_NAME(ConditionalClausePatternUseScope)
DEFINE_GET_CLASS_NAME(CaptureListScope)
DEFINE_GET_CLASS_NAME(WholeClosureScope)
DEFINE_GET_CLASS_NAME(ClosureParametersScope)
DEFINE_GET_CLASS_NAME(ClosureBodyScope)
DEFINE_GET_CLASS_NAME(TopLevelCodeScope)
DEFINE_GET_CLASS_NAME(SpecializeAttributeScope)
DEFINE_GET_CLASS_NAME(SubscriptDeclScope)
DEFINE_GET_CLASS_NAME(VarDeclScope)
DEFINE_GET_CLASS_NAME(IfStmtScope)
DEFINE_GET_CLASS_NAME(WhileStmtScope)
DEFINE_GET_CLASS_NAME(GuardStmtScope)
DEFINE_GET_CLASS_NAME(GuardStmtUseScope)
DEFINE_GET_CLASS_NAME(RepeatWhileScope)
DEFINE_GET_CLASS_NAME(DoCatchStmtScope)
DEFINE_GET_CLASS_NAME(SwitchStmtScope)
DEFINE_GET_CLASS_NAME(ForEachStmtScope)
DEFINE_GET_CLASS_NAME(ForEachPatternScope)
DEFINE_GET_CLASS_NAME(CatchStmtScope)
DEFINE_GET_CLASS_NAME(CaseStmtScope)
DEFINE_GET_CLASS_NAME(BraceStmtScope)

#undef DEFINE_GET_CLASS_NAME

#pragma mark getSourceFile

const SourceFile *ASTScopeImpl::getSourceFile() const {
  return getParent().get()->getSourceFile();
}

const SourceFile *ASTSourceFileScope::getSourceFile() const { return SF; }

SourceRange ExtensionScope::getBraces() const { return decl->getBraces(); }

SourceRange NominalTypeScope::getBraces() const { return decl->getBraces(); }

NullablePtr<NominalTypeDecl>
ExtensionScope::getCorrespondingNominalTypeDecl() const {
  return decl->getExtendedNominal();
}

void ASTScopeImpl::postOrderDo(function_ref<void(ASTScopeImpl *)> fn) {
  for (auto *child : getChildren())
    child->postOrderDo(fn);
  fn(this);
}

ArrayRef<StmtConditionElement> ConditionalClauseScope::getCond() const {
  return stmt->getCond();
}

const StmtConditionElement &
ConditionalClauseScope::getStmtConditionElement() const {
  return getCond()[index];
}
