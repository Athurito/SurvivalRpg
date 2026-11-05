#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/RpgBlueprintAssetTools.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBlueprintAssetToolsReadableGraphTest,
	"SurvivalRpg.Blueprint.AssetTools.AuthorsReadableGraphsAndSockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlueprintAssetToolsReadableGraphTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("BP_ReadableGraphTest")),
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("A transient Blueprint exists"), Blueprint)) return false;
	UEdGraph* Function = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("GetAnswer"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Function, true, nullptr);
	TArray<UK2Node_FunctionEntry*> Entries;
	Function->GetNodesOfClass(Entries);
	if (!TestEqual(TEXT("The function has one entry"), Entries.Num(), 1)) return false;

	// A function gets its tooltip and purity; one the Blueprint does not declare, or an empty tooltip, is refused.
	TestTrue(TEXT("An own function is described"),
		URpgBlueprintAssetTools::DescribeLocalFunction(Blueprint, TEXT("GetAnswer"), TEXT("Returns the answer."), true));
	TestEqual(TEXT("with its tooltip"), Entries[0]->MetaData.ToolTip.ToString(), FString(TEXT("Returns the answer.")));
	TestTrue(TEXT("and as pure"), Entries[0]->HasAnyExtraFlags(FUNC_BlueprintPure));
	TestTrue(TEXT("Purity can be taken back"),
		URpgBlueprintAssetTools::DescribeLocalFunction(Blueprint, TEXT("GetAnswer"), TEXT("Returns the answer."), false)
		&& !Entries[0]->HasAnyExtraFlags(FUNC_BlueprintPure));
	TestFalse(TEXT("A missing function is refused"),
		URpgBlueprintAssetTools::DescribeLocalFunction(Blueprint, TEXT("Missing"), TEXT("Tooltip"), false));
	TestFalse(TEXT("An empty tooltip is refused"), URpgBlueprintAssetTools::DescribeLocalFunction(Blueprint, TEXT("GetAnswer"), FString(), false));

	// A comment box covers its nodes; nodes of another graph or an empty text are refused.
	Entries[0]->NodePosX = 400;
	Entries[0]->NodePosY = -200;
	const UEdGraphNode_Comment* Comment = URpgBlueprintAssetTools::AddGraphComment(Function, { Entries[0] }, TEXT("Answers."));
	if (!TestNotNull(TEXT("A comment is added"), Comment)) return false;
	TestEqual(TEXT("with its text"), Comment->NodeComment, FString(TEXT("Answers.")));
	TestTrue(TEXT("in front of the node's corner"), Comment->NodePosX < 400 && Comment->NodePosY < -200);
	TestTrue(TEXT("and past it"), Comment->NodePosX + Comment->NodeWidth > 400 && Comment->NodePosY + Comment->NodeHeight > -200);
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	TestNull(TEXT("Nodes of another graph are refused"), URpgBlueprintAssetTools::AddGraphComment(EventGraph, { Entries[0] }, TEXT("Wrong graph")));
	TestNull(TEXT("An empty text is refused"), URpgBlueprintAssetTools::AddGraphComment(Function, { Entries[0] }, FString()));
	TestEqual(TEXT("Removing comments removes that comment"), URpgBlueprintAssetTools::RemoveGraphComments(Function), 1);
	TestFalse(TEXT("and keeps the function's own nodes"), Function->Nodes.ContainsByPredicate(
		[](const UEdGraphNode* Node) { return Node && Node->IsA<UEdGraphNode_Comment>(); }) || !Function->Nodes.Contains(Entries[0]));

	// A child component rides a socket of its parent; the root has no parent socket.
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	USCS_Node* Parent = SCS->CreateNode(USkeletalMeshComponent::StaticClass(), TEXT("ParentMesh"));
	USCS_Node* Child = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("ChildMesh"));
	SCS->AddNode(Parent);
	Parent->AddChildNode(Child);
	TestTrue(TEXT("A child component gets a parent socket"),
		URpgBlueprintAssetTools::SetOwnSCSComponentParentSocket(Blueprint, TEXT("ChildMesh"), TEXT("Bow_String")));
	TestEqual(TEXT("and attaches to it"), Child->AttachToName, FName(TEXT("Bow_String")));
	TestFalse(TEXT("A root component is refused"), URpgBlueprintAssetTools::SetOwnSCSComponentParentSocket(Blueprint, TEXT("ParentMesh"), TEXT("Socket")));
	TestFalse(TEXT("A missing component is refused"), URpgBlueprintAssetTools::SetOwnSCSComponentParentSocket(Blueprint, TEXT("Missing"), TEXT("Socket")));
	return true;
}

#endif
