#include "sema_fabricator.hpp"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Sema.h"

using namespace clang;

namespace salt
{

void SaltSemaFabricator::InitializeSema(Sema &S)
{
    sema = &S;
}

// Check if we already created this type
NamedDecl *SaltSemaFabricator::findExisting(DeclContext *DC, IdentifierInfo *II)
{
    for (NamedDecl *D : DC->lookup(DeclarationName(II)))
    {
        return D;
    }
    return nullptr;
}

// Create namespace for encountered unresolved namespace
NamespaceDecl *SaltSemaFabricator::fabricateNamespace(DeclContext *DC,
                                                      IdentifierInfo *II)
{
    ASTContext &Ctx = sema->getASTContext();
    NamespaceDecl *ND =
        NamespaceDecl::Create(Ctx, DC, /*Inline=*/false, SourceLocation(),
                              SourceLocation(), II, /*PrevDecl=*/nullptr,
                              /*Nested=*/false);
    ND->setImplicit(true);
    DC->addDecl(ND);
    ++num_namespaces;
    return ND;
}

// Create class template for encountered unresolved class template
ClassTemplateDecl *SaltSemaFabricator::fabricateClassTemplate(DeclContext *DC,
                                                              IdentifierInfo *II)
{
    ASTContext &Ctx = sema->getASTContext();
    TemplateTypeParmDecl *TTP = TemplateTypeParmDecl::Create(
        Ctx, Ctx.getTranslationUnitDecl(), SourceLocation(), SourceLocation(),
        /*Depth=*/0, /*Position=*/0, /*Id=*/nullptr, /*Typename=*/true,
        /*ParameterPack=*/true);
    NamedDecl *Params[] = {TTP};
    TemplateParameterList *TPL = TemplateParameterList::Create(
        Ctx, SourceLocation(), SourceLocation(), Params, SourceLocation(),
        nullptr);
    CXXRecordDecl *RD = CXXRecordDecl::Create(
        Ctx, TagTypeKind::Class, DC, SourceLocation(), SourceLocation(), II);
    RD->setImplicit(true);
    ClassTemplateDecl *CTD = ClassTemplateDecl::Create(
        Ctx, DC, SourceLocation(), DeclarationName(II), TPL, RD);
    CTD->setImplicit(true);
    RD->setDescribedClassTemplate(CTD);
    DC->addDecl(CTD);
    ++num_templates;
    return CTD;
}

// Create a plain class for an encountered unresolved non-template type
CXXRecordDecl *SaltSemaFabricator::fabricateRecord(DeclContext *DC,
                                                   IdentifierInfo *II)
{
    ASTContext &Ctx = sema->getASTContext();
    CXXRecordDecl *RD = CXXRecordDecl::Create(
        Ctx, TagTypeKind::Class, DC, SourceLocation(), SourceLocation(), II);
    RD->setImplicit(true);
    // Give the class an empty definition
    RD->startDefinition();
    RD->completeDefinition();
    DC->addDecl(RD);
    ++num_records;
    return RD;
}

TypoCorrection SaltSemaFabricator::CorrectTypo(
    const DeclarationNameInfo &Typo, int LookupKind, Scope *S, CXXScopeSpec *SS,
    CorrectionCandidateCallback &CCC, DeclContext *MemberContext,
    bool EnteringContext, const ObjCObjectPointerType *OPT)
{
    IdentifierInfo *II = Typo.getName().getAsIdentifierInfo();
    if (!II || !sema)
    {
        // Returning an empty TypoCorrection makes no corrections.
        return TypoCorrection();
    }
    if (!sema->getLangOpts().CPlusPlus)
    {
        // Namespace/template fabrication is only relevant for C++
        return TypoCorrection();
    }

    DeclContext *DC = sema->getASTContext().getTranslationUnitDecl();
    if (SS && SS->isNotEmpty())
    {
        DeclContext *C = sema->computeDeclContext(*SS, EnteringContext);
        if (!C)
        {
            return TypoCorrection();
        }
        DC = C;
    }

    // Never mutate non-file contexts
    if (!DC->isFileContext())
    {
        return TypoCorrection();
    }

    // On unknown name before '::', fabricate a namespace
    if (LookupKind == Sema::LookupNestedNameSpecifierName)
    {
        NamedDecl *D = findExisting(DC, II);
        if (!D)
        {
            D = fabricateNamespace(DC, II);
        }
        TypoCorrection TC(II);
        TC.addCorrectionDecl(D);
        return TC;
    }

    // Otherwise, fabricate a type for the unknown qualified name.
    if (SS && SS->isNotEmpty())
    {
        NamedDecl *D = findExisting(DC, II);
        if (!D)
        {
            if (CCC.WantTypeSpecifiers)
            {
                D = fabricateRecord(DC, II);
            }
            else
            {
                D = fabricateClassTemplate(DC, II);
            }
        }
        TypoCorrection TC(II);
        TC.addCorrectionDecl(D);
        return TC;
    }
    return TypoCorrection();
}

} // namespace salt
