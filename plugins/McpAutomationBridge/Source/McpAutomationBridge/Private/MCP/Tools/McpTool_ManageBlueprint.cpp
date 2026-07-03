// McpTool_ManageBlueprint.cpp — canonical manage_blueprint tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageBlueprint : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_blueprint"); }

	FString GetDescription() const override
	{
		return TEXT("Create Blueprints, add SCS components (mesh, collision, camera), "
			"and manipulate graph nodes.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::ManageBlueprint(),
					TEXT("Blueprint action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("blueprintType"), TEXT("Path or name of the parent class."))
			.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
			.Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.Bool(TEXT("waitForCompletion"), TEXT("create/create_blueprint: wait for a coalesced concurrent creation to finish before responding."))
			.String(TEXT("componentType"), TEXT(""))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("componentClass"), TEXT(""))
			.String(TEXT("attachTo"), TEXT(""))
			.String(TEXT("newParent"), TEXT(""))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.String(TEXT("variableName"), TEXT("Name of the variable."))
			.String(TEXT("oldName"), TEXT(""))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.String(TEXT("graphName"), TEXT("Name of the graph."))
			.String(TEXT("nodeType"), TEXT(""))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("nodeName"), TEXT("add_node: name for a CustomEvent node (nodeType=CustomEvent)."))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("memberName"), TEXT(""))
			.Number(TEXT("x"), TEXT(""))
			.Number(TEXT("y"), TEXT(""))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x"), TEXT("X coordinate."))
				 .Number(TEXT("y"), TEXT("Y coordinate."))
				 .Number(TEXT("z"), TEXT("Z coordinate."))
				 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch"), TEXT("Pitch."))
				 .Number(TEXT("yaw"), TEXT("Yaw."))
				 .Number(TEXT("roll"), TEXT("Roll."))
				 .Required({TEXT("pitch"), TEXT("yaw"), TEXT("roll")});
			})
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x"), TEXT("X scale."))
				 .Number(TEXT("y"), TEXT("Y scale."))
				 .Number(TEXT("z"), TEXT("Z scale."))
				 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
			})
			.ArrayOfObjects(TEXT("operations"), TEXT(""))
			.Bool(TEXT("compile"), TEXT("Compile the blueprint(s) after the operation."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.String(TEXT("eventType"), TEXT(""))
			.String(TEXT("customEventName"), TEXT("Name of the event."))
			.ArrayOfObjects(TEXT("parameters"), TEXT(""))
			.String(TEXT("variableType"),
				TEXT("Variable type (e.g., Boolean, Float, Integer, Vector, String, Object)"))
			.FreeformObject(TEXT("defaultValue"), TEXT("Generic value (any type)."))
			.String(TEXT("category"), TEXT(""))
			.Bool(TEXT("isReplicated"), TEXT(""))
			.Bool(TEXT("isPublic"), TEXT(""))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.ArrayOfObjects(TEXT("inputs"), TEXT(""))
			.ArrayOfObjects(TEXT("outputs"), TEXT(""))
			.Bool(TEXT("override"), TEXT("add_function: implement an inherited BlueprintImplementableEvent/BlueprintNativeEvent instead of creating a new function."))
			.Number(TEXT("posX"), TEXT("Node X position (graph units)."))
			.Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
			.Bool(TEXT("autoConnect"), TEXT("add_node: auto-wire the new node's exec-in to an open event chain (default false)."))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("parentComponent"), TEXT(""))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("memberClass"), TEXT(""))
			.String(TEXT("targetClass"), TEXT(""))
			.String(TEXT("inputAxisName"), TEXT(""))
			.Bool(TEXT("saveAfterCompile"), TEXT(""))
			.String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
			.Bool(TEXT("createIfMissing"), TEXT("ensure_exists: create the blueprint if it doesn't exist (default true)."))
			.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
			.String(TEXT("sourceNodeId"), TEXT("ID of the source node."))
			.String(TEXT("sourcePinName"), TEXT("Name of the source pin."))
			.String(TEXT("targetNodeId"), TEXT("ID of the target node."))
			.String(TEXT("targetPinName"), TEXT("Name of the target pin."))
			// Widget-authoring (UMG) navigation params — this tool re-dispatches the widget
			// action group. Handlers already read them from the payload; declared here so a
			// schema-validating client can discover/pass them (per-widget value props still
			// go through the generic `value`/`properties` fields).
			.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
			.String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget)."))
			.String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
			.String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
			.String(TEXT("widgetClass"), TEXT("Widget class for create_widget_blueprint / set_widget_parent_class."))
			.Number(TEXT("anchorMinX"), TEXT("set_anchor: anchor minimum X (0..1)."))
			.Number(TEXT("anchorMinY"), TEXT("set_anchor: anchor minimum Y (0..1)."))
			.Number(TEXT("anchorMaxX"), TEXT("set_anchor: anchor maximum X (0..1)."))
			.Number(TEXT("anchorMaxY"), TEXT("set_anchor: anchor maximum Y (0..1)."))
			.String(TEXT("preset"), TEXT("set_anchor: named anchor preset (TopLeft, TopCenter, TopRight, CenterLeft, Center, CenterRight, BottomLeft, BottomCenter, BottomRight, StretchHorizontal, StretchVertical, StretchAll)."))
			// Common UI (add_common_button/add_common_text/add_common_border/set_common_*_style/
			// create_common_*_style/set_common_button_input_action) — handlers read these directly.
			.String(TEXT("buttonClass"), TEXT("add_common_button: concrete UCommonButtonBase subclass path."))
			.String(TEXT("styleClass"), TEXT("Common UI style class (add_common_button/add_common_text/add_common_border/set_common_button_style/set_common_text_style)."))
			.String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
			.String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."))
			.String(TEXT("dataTable"), TEXT("set_common_button_input_action: DataTable asset path (CommonInputActionDataBase row struct)."))
			.String(TEXT("rowName"), TEXT("set_common_button_input_action: row name within dataTable."))
			// Widget lifecycle / tree mutation extras.
			.String(TEXT("widgetId"), TEXT("show_widget: special id (\"notification\") or target identifier."))
			.String(TEXT("message"), TEXT("show_widget: notification text when widgetId is \"notification\"."))
			.String(TEXT("newWidgetClass"), TEXT("replace_widget_class: class to swap the named widget to."))
			.String(TEXT("panelType"), TEXT("wrap_root: new root panel class (Overlay, VerticalBox, HorizontalBox, CanvasPanel)."))
			.String(TEXT("parentName"), TEXT("Name of an existing panel widget to attach into (add_widget_component, add_health_bar/add_crosshair/add_minimap/etc.)."))
			.String(TEXT("title"), TEXT("create_main_menu: title text (default \"Main Menu\")."))
			// Panel-construction params (add_uniform_grid/add_wrap_box/add_scroll_box/add_size_box/add_scale_box).
			.Object(TEXT("slotPadding"), TEXT("add_uniform_grid: per-slot padding."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("left")).Number(TEXT("top")).Number(TEXT("right")).Number(TEXT("bottom"));
			})
			.Number(TEXT("minDesiredSlotWidth"), TEXT("add_uniform_grid: minimum slot width."))
			.Number(TEXT("minDesiredSlotHeight"), TEXT("add_uniform_grid: minimum slot height."))
			.Object(TEXT("innerSlotPadding"), TEXT("add_wrap_box: inner slot padding."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("wrapSize"), TEXT("add_wrap_box: explicit wrap size (UE 5.1+)."))
			.String(TEXT("orientation"), TEXT("add_scroll_box: \"Vertical\" (default) or \"Horizontal\"."))
			.String(TEXT("scrollBarVisibility"), TEXT("add_scroll_box: Visible, Collapsed, or Hidden."))
			.Bool(TEXT("alwaysShowScrollbar"), TEXT("add_scroll_box: force the scrollbar visible."))
			.Number(TEXT("widthOverride"), TEXT("add_size_box: fixed width override."))
			.Number(TEXT("heightOverride"), TEXT("add_size_box: fixed height override."))
			.Number(TEXT("minDesiredWidth"), TEXT("add_size_box: minimum desired width."))
			.Number(TEXT("minDesiredHeight"), TEXT("add_size_box: minimum desired height."))
			.Number(TEXT("maxDesiredWidth"), TEXT("add_size_box: maximum desired width."))
			.Number(TEXT("maxDesiredHeight"), TEXT("add_size_box: maximum desired height."))
			.String(TEXT("stretch"), TEXT("add_scale_box: stretch mode (None, Fill, ScaleToFit, ScaleToFitX, ScaleToFitY, ScaleToFill, UserSpecified)."))
			.Number(TEXT("userSpecifiedScale"), TEXT("add_scale_box: scale used when stretch is UserSpecified."))
			.String(TEXT("stretchDirection"), TEXT("add_scale_box: Both, DownOnly, or UpOnly."))
			.Number(TEXT("sizeX"), TEXT("add_spacer/add_widget_component: width in pixels."))
			.Number(TEXT("sizeY"), TEXT("add_spacer/add_widget_component: height in pixels."))
			// Leaf-widget construction properties (add_text_block/add_image/add_button/
			// add_progress_bar/add_slider/add_check_box/add_text_input/add_combo_box/add_spin_box).
			.String(TEXT("text"), TEXT("Initial text (add_common_text, add_text_block, add_rich_text_block, add_interaction_prompt, add_widget_component)."))
			.Number(TEXT("fontSize"), TEXT("Font size (add_text_block, set_font)."))
			.Bool(TEXT("autoWrap"), TEXT("add_text_block: enable auto text wrapping."))
			.Object(TEXT("colorAndOpacity"), TEXT("Widget tint color, 0-1 components (add_text_block, add_image, add_button)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a"));
			})
			.String(TEXT("texturePath"), TEXT("add_image: texture asset path for the brush."))
			.Bool(TEXT("isEnabled"), TEXT("add_button: initial enabled state."))
			.Number(TEXT("percent"), TEXT("add_progress_bar: initial fill percent (0-1)."))
			.Object(TEXT("fillColorAndOpacity"), TEXT("add_progress_bar: fill bar color, 0-1 components."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a"));
			})
			.Bool(TEXT("isMarquee"), TEXT("add_progress_bar: indeterminate marquee mode."))
			.Number(TEXT("minValue"), TEXT("add_slider/add_spin_box: minimum value."))
			.Number(TEXT("maxValue"), TEXT("add_slider/add_spin_box: maximum value."))
			.Number(TEXT("stepSize"), TEXT("add_slider: step increment."))
			.Bool(TEXT("isChecked"), TEXT("add_check_box: initial checked state."))
			.Bool(TEXT("multiLine"), TEXT("add_text_input: use a multi-line editable text box."))
			.String(TEXT("hintText"), TEXT("add_text_input: placeholder hint text."))
			.Array(TEXT("options"), TEXT("add_combo_box: option strings to populate."))
			.String(TEXT("selectedOption"), TEXT("add_combo_box: initially selected option."))
			.Number(TEXT("delta"), TEXT("add_spin_box: increment/decrement step."))
			// Slot layout (set_anchor/set_alignment/set_position/set_size/set_padding/
			// set_z_order/set_render_transform/set_visibility/set_style/set_clipping).
			.Object(TEXT("anchorMin"), TEXT("set_anchor: minimum anchor (alternative to anchorMinX/anchorMinY)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("anchorMax"), TEXT("set_anchor: maximum anchor (alternative to anchorMaxX/anchorMaxY)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("alignment"), TEXT("set_alignment: pivot (alternative to alignmentX/alignmentY)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("alignmentX"), TEXT("set_alignment: pivot X (0..1)."))
			.Number(TEXT("alignmentY"), TEXT("set_alignment: pivot Y (0..1)."))
			.Object(TEXT("position"), TEXT("set_position: Canvas slot position (alternative to posX/posY)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.FreeformObject(TEXT("size"), TEXT("Either a number (add_crosshair/add_minimap/add_ammo_counter: icon size) or an {x,y} object (set_size: canvas slot pixel size)."))
			.Bool(TEXT("fill"), TEXT("set_size: stretch a HBox/VBox child to fill (vs Automatic)."))
			.Number(TEXT("fillWeight"), TEXT("set_size: fill weight when fill is true."))
			.Object(TEXT("padding"), TEXT("set_padding: padding (alternative to pad/padLeft/padTop/padRight/padBottom)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("left")).Number(TEXT("top")).Number(TEXT("right")).Number(TEXT("bottom"));
			})
			.Number(TEXT("pad"), TEXT("set_padding: uniform padding on all sides."))
			.Number(TEXT("padLeft"), TEXT("set_padding: left padding."))
			.Number(TEXT("padTop"), TEXT("set_padding: top padding."))
			.Number(TEXT("padRight"), TEXT("set_padding: right padding."))
			.Number(TEXT("padBottom"), TEXT("set_padding: bottom padding."))
			.Number(TEXT("zOrder"), TEXT("set_z_order: Canvas slot Z-order."))
			.Object(TEXT("translation"), TEXT("set_render_transform: 2D translation."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("shear"), TEXT("set_render_transform: 2D shear."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("angle"), TEXT("set_render_transform: rotation angle in degrees."))
			.String(TEXT("visibility"), TEXT("set_visibility: Visible, Hidden, Collapsed, HitTestInvisible, or SelfHitTestInvisible."))
			.String(TEXT("clipping"), TEXT("set_clipping: Inherit, ClipToBounds, ClipToBoundsWithoutIntersecting, ClipToBoundsAlways, or OnDemand."))
			.String(TEXT("style"), TEXT("set_style: legacy value alias for propertyName=\"Style\" when propertyName is omitted."))
			.String(TEXT("font"), TEXT("set_font: font asset path."))
			.Number(TEXT("left"), TEXT("set_margin: left margin."))
			.Number(TEXT("top"), TEXT("set_margin: top margin."))
			.Number(TEXT("right"), TEXT("set_margin: right margin."))
			.Number(TEXT("bottom"), TEXT("set_margin: bottom margin."))
			// Event / delegate bindings.
			.String(TEXT("delegateName"), TEXT("Multicast delegate name (bind_on_clicked/bind_on_hovered default OnClicked/OnHovered; bind_on_value_changed default OnValueChanged; bind_event_to_delegate)."))
			.String(TEXT("targetFunction"), TEXT("bind_on_clicked/bind_on_hovered: self UFUNCTION to call (alias functionName)."))
			.String(TEXT("targetText"), TEXT("bind_on_value_changed: text widget to live-update with the new value."))
			.String(TEXT("ownerPin"), TEXT("bind_event_to_delegate: output pin on the source event exposing the delegate's owner object."))
			.String(TEXT("handlerEventName"), TEXT("bind_event_to_delegate: name for the generated handler custom event (default On_<delegateName>)."))
			.String(TEXT("handlerTargetWidget"), TEXT("bind_event_to_delegate: optional widget the handler acts on."))
			.String(TEXT("handlerFunction"), TEXT("bind_event_to_delegate: optional function the handler calls."))
			.String(TEXT("targetWidget"), TEXT("set_widget_binding: name of the widget to bind (NOT_IMPLEMENTED — validated only)."))
			.String(TEXT("property"), TEXT("set_widget_binding: property name to bind (NOT_IMPLEMENTED — validated only)."))
			// Localization.
			.String(TEXT("stringTableId"), TEXT("bind_localized_text: string table asset path."))
			.String(TEXT("stringKey"), TEXT("bind_localized_text: key within the string table."))
			.String(TEXT("namespace"), TEXT("set_localization_key: localization namespace (default \"Game\")."))
			.String(TEXT("key"), TEXT("set_localization_key: localization key."))
			// Animation / style.
			.String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
			.Number(TEXT("duration"), TEXT("create_widget_animation: length in seconds (default 1.0)."))
			.String(TEXT("styleName"), TEXT("create_widget_style/apply_style_to_widget: style variable name."))
			.String(TEXT("styleType"), TEXT("create_widget_style: style category (default \"Text\")."))
			// HUD composite widgets.
			.Number(TEXT("width"), TEXT("add_health_bar: bar width in pixels."))
			.Number(TEXT("height"), TEXT("add_health_bar: bar height in pixels."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageBlueprint);
