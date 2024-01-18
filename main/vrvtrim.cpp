
/*
 * Convert SVG from Verovio (SVG-1.1 full) to profile supported by Qt
 * (SVG-1.2 Tiny). This code was proposed for use with Macaw
 * (https://github.com/exyte/Macaw, MIT license) by Alpha (alpha0010)
 * https://github.com/exyte/Macaw/issues/759#issuecomment-1022216541
 */

#include "vrvtrim.h"

#include "pugixml.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

template <class UnaryPredicate>
inline static void vrvSvgTrim(std::string &s, UnaryPredicate p)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), p).base(), s.end());
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), p));
}

/**
 * Remove alphabetical characters from both ends of the string.
 */
inline static void vrvSvgTrimLetters(std::string &s)
{
    vrvSvgTrim(s, [](int c) { return !std::isalpha(c); });
}

/**
 * Verovio has an svg element as a child of the root svg. Flatten it out.
 */
static void removeInnerSvg(const pugi::xml_document &svgXml)
{
    pugi::xml_node root = svgXml.first_child();
    pugi::xml_node innerSvg = root.child("svg");
    // Promote required attributes.
    root.append_attribute("viewBox") = innerSvg.attribute("viewBox").value();
    root.remove_attribute("width");
    root.remove_attribute("height");

    // Promote children.
    for (pugi::xml_node node = innerSvg.first_child(); node;
         node = innerSvg.first_child()) {
        root.append_move(node);
    }
    root.remove_child(innerSvg);
}

/**
 * Merge whitelisted attributes from the element with the supplied mapping.
 */
static std::map<std::string, std::string> mergeTextAttributes(
    pugi::xml_node child,
    std::map<std::string, std::string> parentAttr)
{
    std::vector<std::string> knownAttrs{
        "x",
        "y",
        "font-family",
        "font-size",
        "font-style",
        "text-anchor",
        "class"};
    std::map<std::string, std::string> output;
    for (const std::string &attr : knownAttrs) {
        pugi::xml_attribute childAttr = child.attribute(attr.c_str());
        if (!childAttr.empty()) {
            output[attr] = childAttr.value();
        } else if (parentAttr.find(attr) != parentAttr.end()) {
            // Attribute not in child? Take forwarded from parent, if exists.
            output[attr] = parentAttr[attr];
        }
    }
    return output;
}

/**
 * Populate a flattened tspan element.
 */
static int appendFlatTspan(
    pugi::xml_node flatTextNode,
    pugi::xml_node elem,
    std::map<std::string, std::string> newAttrs)
{
    // Try to merge nodes.
    pugi::xml_node prevTspan = flatTextNode.last_child();
    if (std::strcmp(prevTspan.name(), "tspan") == 0) {
        int attrMatchCount = 0;
        for (pugi::xml_attribute attr : prevTspan.attributes()) {
            auto itr = newAttrs.find(attr.name());
            if (itr != newAttrs.end() && itr->second == attr.value()) {
                attrMatchCount += 1;
            } else {
                attrMatchCount = -1;
                break;
            }
        }
        if (newAttrs.size() == attrMatchCount) {
            // Previous node has the same attributes. Append text children
            // nodes instead of wrapping in a new tspan.
            std::string combinedText(prevTspan.text().get());
            combinedText += elem.text().get();
            prevTspan.remove_children();
            prevTspan.append_child(pugi::node_pcdata)
                .set_value(combinedText.c_str());
            return 0;
        }
    }

    // Append new node.
    pugi::xml_node textNode = flatTextNode.append_child("tspan");
    textNode.append_child(pugi::node_pcdata).set_value(elem.text().get());
    for (const auto &attr : newAttrs) {
        textNode.append_attribute(attr.first.c_str()) = attr.second.c_str();
    }
    return 1;
}

/**
 * Recursively reduce nested tspan elements to a single layer.
 *
 * @param flatTextNode
 *  Target node within which to create new elements.
 * @param elem
 *  tspan element to flatten.
 *
 * @return
 *  Number of nodes created.
 */
static int recurseFlattenTextNode(
    pugi::xml_node flatTextNode,
    pugi::xml_node elem,
    const std::map<std::string, std::string> &parentAttr)
{
    int numAdded = 0;
    bool hasChild = false;
    for (pugi::xml_node child : elem.children("tspan")) {
        hasChild = true;
        numAdded += recurseFlattenTextNode(
            flatTextNode,
            child,
            mergeTextAttributes(child, parentAttr));
    }
    if (!hasChild && !elem.text().empty()) {
        numAdded += appendFlatTspan(flatTextNode, elem, parentAttr);
    }
    return numAdded;
}

/**
 * Recursively reduce nested tspan elements to a single layer.
 */
static void removeNestedTspan(const pugi::xml_document &svgXml)
{
    for (pugi::xpath_node selectedNode :
         svgXml.select_nodes("//*[local-name()=\"text\"]")) {
        pugi::xml_node node = selectedNode.node();

        pugi::xml_node flatTextNode =
            node.parent().insert_copy_after(node, node);
        flatTextNode.remove_children();
        if (recurseFlattenTextNode(
                flatTextNode,
                node,
                std::map<std::string, std::string>()) > 0) {
            node.parent().remove_child(node);
        } else {
            node.parent().remove_child(flatTextNode);
        }
    }
}

/**
 * Set text font.
 */
static void styleVerseText(const pugi::xml_document &svgXml)
{
    pugi::xml_node style = svgXml.first_child().append_child("style");
    style.append_attribute("type") = "text/css";
    style.text() = ".text { font-family: LiberationSerif; }";
}

/**
 * Convert svg symbol defs to path defs, and other manipulations
 * needed to conform to SVG 1.2 Tiny.
 */
std::string
VrvTrim::transformSvgToTiny(const std::string &svg)
{
    pugi::xml_document svgXml;
    pugi::xml_parse_result parseResult = svgXml.load_string(svg.c_str());
    if (parseResult.status != pugi::status_ok) {
        return parseResult.description();
    }

    // Find symbol usages.
    std::map<std::string, std::set<std::string>> useConfigs;
    for (pugi::xpath_node selectedNode :
         svgXml.select_nodes("//*[local-name()=\"use\"]")) {
        pugi::xml_node node = selectedNode.node();

        pugi::xml_attribute hrefAttr = node.attribute("xlink:href");
        std::string oldHref(hrefAttr.value());
        std::string width(node.attribute("width").value());
        vrvSvgTrimLetters(width);
        std::string height(node.attribute("height").value());
        vrvSvgTrimLetters(height);

        // Retarget to a path def.
        std::string elemConfig(width + '-' + height);
        std::string newHref(oldHref + '-' + elemConfig);
        hrefAttr.set_value(newHref.c_str());
        node.remove_attribute("width");
        node.remove_attribute("height");

        std::string symbolId(oldHref.substr(1));
        useConfigs[symbolId].insert(elemConfig);
    }

    // Create path defs.
    pugi::xml_node defs = svgXml.first_child().child("defs");
    std::regex transformRe(R"(scale\((\d+),\s*(-?\d+)\))");
    std::regex viewBoxRe(R"(\d+\s+\d+\s+(\d+)\s+(\d+))");
    for (pugi::xpath_node selectedNode :
         svgXml.select_nodes("//*[local-name()=\"symbol\"]")) {
        pugi::xml_node node = selectedNode.node();

        std::string symbolId(node.attribute("id").value());
        std::string viewBox(node.attribute("viewBox").value());
        pugi::xml_node pathElem = node.child("path");
        std::string transform(pathElem.attribute("transform").value());
        std::string coords(pathElem.attribute("d").value());

        // Remove symbol def.
        node.parent().remove_child(node);

        // Parse attributes.
        std::smatch parsedTransform;
        if (!std::regex_search(transform, parsedTransform, transformRe)) {
            continue;
        }
        std::smatch parsedViewBox;
        if (!std::regex_search(viewBox, parsedViewBox, viewBoxRe)) {
            continue;
        }
        double transformX = std::stod(parsedTransform[1]);
        double transformY = std::stod(parsedTransform[2]);
        double viewBoxWidth = std::stod(parsedViewBox[1]);
        double viewBoxHeight = std::stod(parsedViewBox[2]);

        // Create def nodes for each required transform of the symbol.
        for (const std::string &elemConfig : useConfigs[symbolId]) {
            size_t dashIdx = elemConfig.find('-');
            double width = std::stod(elemConfig.substr(0, dashIdx));
            double height = std::stod(elemConfig.substr(dashIdx + 1));

            pugi::xml_node newPathElem = defs.append_child("path");
            newPathElem.append_attribute("id") =
                (symbolId + '-' + elemConfig).c_str();

            std::string transformAttr("scale(");
            transformAttr += std::to_string(transformX * width / viewBoxWidth);
            transformAttr += ',';
            transformAttr +=
                std::to_string(transformY * height / viewBoxHeight);
            transformAttr += ')';
            newPathElem.append_attribute("transform") = transformAttr.c_str();
            newPathElem.append_attribute("d") = coords.c_str();
        }
    }

    removeNestedTspan(svgXml);
    removeInnerSvg(svgXml);
    styleVerseText(svgXml);

    std::ostringstream result;
    svgXml.save(result);
    return result.str();
}
