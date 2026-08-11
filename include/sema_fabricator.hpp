#ifndef SEMA_FABRICATOR_HPP
#define SEMA_FABRICATOR_HPP

#include "clang/Sema/ExternalSemaSource.h"
#include "clang/Sema/TypoCorrection.h"

namespace salt
{

// An ExternalSemaSource that fabricates declarations for unknown qualified names during parsing
class SaltSemaFabricator : public clang::ExternalSemaSource
{
public:
    void InitializeSema(clang::Sema &S) override;

    clang::TypoCorrection CorrectTypo(const clang::DeclarationNameInfo &Typo,
                                      int LookupKind, clang::Scope *S,
                                      clang::CXXScopeSpec *SS,
                                      clang::CorrectionCandidateCallback &CCC,
                                      clang::DeclContext *MemberContext,
                                      bool EnteringContext,
                                      const clang::ObjCObjectPointerType *OPT) override;

    unsigned numNamespaces() const { return num_namespaces; }
    unsigned numTemplates() const { return num_templates; }

private:
    clang::NamedDecl *findExisting(clang::DeclContext *DC,
                                   clang::IdentifierInfo *II);
    clang::NamespaceDecl *fabricateNamespace(clang::DeclContext *DC,
                                             clang::IdentifierInfo *II);
    clang::ClassTemplateDecl *fabricateClassTemplate(clang::DeclContext *DC,
                                                     clang::IdentifierInfo *II);

    clang::Sema *sema = nullptr;
    unsigned num_namespaces = 0;
    unsigned num_templates = 0;
};

} // namespace salt

#endif
