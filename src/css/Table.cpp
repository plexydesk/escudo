/*
 * Copyright 2011-2013 Esrille Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Table.h"

#include <org/w3c/dom/html/HTMLTableElement.h>
#include <org/w3c/dom/html/HTMLTableCaptionElement.h>
#include <org/w3c/dom/html/HTMLTableCellElement.h>
#include <org/w3c/dom/html/HTMLTableColElement.h>
#include <org/w3c/dom/html/HTMLTableRowElement.h>
#include <org/w3c/dom/html/HTMLTableSectionElement.h>

#include "CSSPropertyValueImp.h"
#include "DocumentImp.h"
#include "FormattingContext.h"
#include "ViewCSSImp.h"

namespace org { namespace w3c { namespace dom { namespace bootstrap {

CellBox::CellBox(Element element, const CSSStyleDeclarationPtr& style):
    Block(element, style),
    fixedLayout(false),
    col(0),
    row(0),
    colSpan(1),
    rowSpan(1),
    verticalAlign(CSSVerticalAlignValueImp::Baseline),  // see TableWrapperBox::processCell
    intrinsicHeight(0.0f),
    columnWidth(NAN)
{
    if (style)
        verticalAlign = style->verticalAlign.getValueForCell();
}

void CellBox::fit(float w, FormattingContext* context)
{
    if (getBlockWidth() == w)
        return;
    width = w - getBlankLeft() - getBlankRight();
    for (BoxPtr child = getFirstChild(); child; child = child->getNextSibling())
        child->fit(width, context);
}

void CellBox::separateBorders(const CSSStyleDeclarationPtr& style, unsigned xWidth, unsigned yHeight)
{
    float hs = style->borderSpacing.getHorizontalSpacing();
    float vs = style->borderSpacing.getVerticalSpacing();
    marginTop = (row == 0) ? vs : (vs / 2.0f);
    marginRight = (col + colSpan == xWidth) ? hs : (hs / 2.0f);
    marginBottom = (row + rowSpan == yHeight) ? vs : (vs / 2.0f);
    marginLeft = (col == 0) ? hs : (hs / 2.0f);
}

void CellBox::collapseBorder(const TableWrapperBoxPtr& wrapper)
{
    borderTop = borderRight = borderBottom = borderLeft = 0.0f;
    marginTop = wrapper->getRowBorderValue(col, row)->getWidth() / 2.0f;
    marginRight = wrapper->getColumnBorderValue(col + 1, row)->getWidth() / 2.0f;
    marginBottom = wrapper->getRowBorderValue(col, row + 1)->getWidth() / 2.0f;
    marginLeft = wrapper->getColumnBorderValue(col, row)->getWidth() / 2.0f;
}

float CellBox::getBaseline(const BoxPtr& box) const
{
    float baseline = box->getBlankTop();
    for (BoxPtr i = box->getFirstChild(); i; i = i->getNextSibling()) {
        if (auto table = std::dynamic_pointer_cast<TableWrapperBox>(i))
            return baseline + table->getBaseline();
        else if (auto block = std::dynamic_pointer_cast<Block>(i)) {
            float x = getBaseline(block);
            if (!isnan(x))
                return baseline + x;
        } else if (auto lineBox = std::dynamic_pointer_cast<LineBox>(i)) {
            if (lineBox->hasInlineBox())
                return baseline + lineBox->getBaseline();
        }
        baseline += i->getTotalHeight();
        if (box->height != 0.0f || !std::dynamic_pointer_cast<LineBox>(box->getFirstChild()))
            baseline += i->getClearance();
    }
    return NAN;
}

float CellBox::getBaseline() const
{
    float x = getBaseline(self());
    return !isnan(x) ? x : (getBlankTop() + height);
}

float CellBox::adjustWidth()
{
    auto wrapper(std::dynamic_pointer_cast<TableWrapperBox>(getParentBox()->getParentBox()->getParentBox()));
    assert(wrapper);
    if (wrapper->getStyle()->borderCollapse.getValue() == CSSBorderCollapseValueImp::Collapse)
        collapseBorder(wrapper);
    else
        separateBorders(wrapper->getStyle(), wrapper->getColumnCount(), wrapper->getRowCount());
    float w = isnan(columnWidth) ? getTotalWidth() : columnWidth;
    width = w - getBlankLeft() - getBlankRight();
    if (fixedLayout || isnan(columnWidth))
        return columnWidth;

    width = columnWidth - getBlankLeft() - getBlankRight();
    for (BoxPtr i = getFirstChild(); i; i = i->getNextSibling())
        i->unresolveStyle();
    return width;
}

float CellBox::shrinkTo()
{
    if (fixedLayout)
        return getBlockWidth();

    float min = 0.0f;
    if (!isAnonymous() && style && !style->width.isAuto())
        min = style->width.getPx();
    for (BoxPtr child = getFirstChild(); child; child = child->getNextSibling())
        min = std::max(min, child->shrinkTo());
    return min + getBlankLeft() + getBlankRight();
}

float CellBox::resolveWidth(float w, FormattingContext* context, float r)
{
    if (fixedLayout) {
        w -= borderLeft + paddingLeft + paddingRight + borderRight;
        width = w;
        return 0.0f;
    }
    return Block::resolveWidth(w, context, r);
}

bool CellBox::isEmptyCell() const
{
    if (hasChildBoxes() || !style)
        return false;
    if (style->emptyCells.getValue() == CSSEmptyCellsValueImp::Show)
        return false;
    return style->borderCollapse.getValue();
}

void CellBox::render(ViewCSSImp* view, StackingContext* stackingContext)
{
    if (isEmptyCell())
        return;
    unsigned overflow = renderBegin(view, true);
    renderInline(view, stackingContext);
    renderEnd(view, overflow);
}

void CellBox::renderNonInline(ViewCSSImp* view, StackingContext* stackingContext)
{
    if (isEmptyCell())
        return;
    unsigned overflow = renderBegin(view);
    Block::renderNonInline(view, stackingContext);
    renderEnd(view, overflow, false);
}

TableWrapperBox::TableWrapperBox(ViewCSSImp* view, Element element, const CSSStyleDeclarationPtr& style) :
    Block((style->display == CSSDisplayValueImp::Table || style->display == CSSDisplayValueImp::InlineTable) ? element : nullptr,
          (style->display == CSSDisplayValueImp::Table || style->display == CSSDisplayValueImp::InlineTable) ? style : nullptr),
    view(view),
    table(element),
    xWidth(0),
    yHeight(0),
    isAnonymousTable(style->display != CSSDisplayValueImp::Table && style->display != CSSDisplayValueImp::InlineTable),
    inRow(false),
    xCurrent(0),
    yCurrent(0),
    yTheadBegin(0),
    yTheadEnd(0),
    yTfootBegin(0),
    yTfootEnd(0)
{
    if (!stackingContext)
        stackingContext = style->getStackingContext();
    isHtmlTable = html::HTMLTableElement::hasInstance(element);
    if (isAnonymousTable)
        processTableChild(element, style);
}

TableWrapperBox::~TableWrapperBox()
{
}

unsigned TableWrapperBox::appendRow()
{
    ++yHeight;
    grid.resize(yHeight);
    grid.back().resize(xWidth);
    rows.resize(yHeight);
    rowGroups.resize(yHeight);
    return yHeight;
}

unsigned TableWrapperBox::appendColumn()
{
    ++xWidth;
    for (auto r = grid.begin(); r != grid.end(); ++r)
        r->resize(xWidth);
    columns.resize(xWidth);
    columnGroups.resize(xWidth);
    return xWidth;
}

void TableWrapperBox::constructBlocks()
{
    processHeader();
    processFooter();

    // Top caption boxes
    for (auto i = topCaptions.begin(); i != topCaptions.end(); ++i)
        appendChild(*i);

    // Table box
    tableBox = std::make_shared<Block>(getNode(), getStyle());
    if (auto box = getTableBox()) {
        for (unsigned y = 0; y < yHeight; ++y) {
            LineBoxPtr lineBox = std::make_shared<LineBox>(nullptr);
            if (!lineBox)
                continue;
            box->appendChild(lineBox);
            for (unsigned x = 0; x < xWidth; ++x) {
                CellBoxPtr cellBox = grid[y][x];
                if (!cellBox || cellBox->isSpanned(x, y))
                    continue;
                cellBox->resetWidth();
                lineBox->appendChild(cellBox);
            }
        }
        appendChild(box);
    }

    // Bottom caption boxes
    for (auto i = bottomCaptions.begin(); i != bottomCaptions.end(); ++i)
        appendChild(*i);

    isAnonymousTable = false;

    clearFlags(NEED_EXPANSION | Box::NEED_CHILD_EXPANSION);
    setFlags(NEED_REFLOW);
}

void TableWrapperBox::clearGrid()
{
    // TODO: Except for blockMap, initialize data members for the upcoming block reconstruction.
    removeChildren();
    tableBox.reset();

    topCaptions.clear();
    bottomCaptions.clear();

    rows.clear();
    rowGroups.clear();
    columns.clear();
    columnGroups.clear();

    grid.clear();
    xWidth = 0;
    yHeight = 0;

    borderRows.clear();
    borderColumns.clear();

    widths.clear();
    fixedWidths.clear();
    percentages.clear();
    heights.clear();
    baselines.clear();
    rowImages.clear();
    rowGroupImages.clear();
    columnImages.clear();
    columnGroupImages.clear();

    isAnonymousTable = !style || (style->display != CSSDisplayValueImp::Table && style->display != CSSDisplayValueImp::InlineTable);

    // CSS table model
    inRow = false;
    xCurrent = 0;
    yCurrent = 0;
    anonymousCell = 0;
    anonymousTable = 0;
    pendingTheadElement = nullptr;
    yTheadBegin = 0;
    yTheadEnd = 0;
    pendingTfootElement = nullptr;
    yTfootBegin = 0;
    yTfootEnd = 0;
}

BlockPtr TableWrapperBox::constructTablePart(Node node)
{
    assert(view);
    BlockPtr part = view->constructBlock(node, nullptr, nullptr, nullptr, true);
    if (part) {
        part->stackingContext = stackingContext;
        if (style)  // Do not support the incremental reflow with an anonymous table
            addBlock(node, part);
    }
    return part;
}

void TableWrapperBox::revertTablePart(Node node)
{
    if (Element::hasInstance(node)) {
        Element element(interface_cast<Element>(node));
        CSSStyleDeclarationPtr style = view->getStyle(element);
        if (style) {
            switch (style->display.getValue()) {
            case CSSDisplayValueImp::TableRowGroup:
            case CSSDisplayValueImp::TableHeaderGroup:
            case CSSDisplayValueImp::TableFooterGroup:
                for (Element i = element.getFirstElementChild(); i; i = i.getNextElementSibling()) {
                    CSSStyleDeclarationPtr s = view->getStyle(i);
                    if (s && s->display.getValue() == CSSDisplayValueImp::TableRow) {
                        for (Element j = i.getFirstElementChild(); j; j = j.getNextElementSibling())
                            removeBlock(j);
                    }
                }
                break;
            case CSSDisplayValueImp::TableRow:
                for (Element i = element.getFirstElementChild(); i; i = i.getNextElementSibling())
                    removeBlock(i);
                break;
            default:
                break;
            }
        }
    }
    removeBlock(node);
    setFlags(Box::NEED_EXPANSION | Box::NEED_REFLOW);
}

void TableWrapperBox::reconstructBlocks()
{
    assert(view);
    for (BoxPtr child = getFirstChild(); child; child = child->getNextSibling()) {
        if (child->getFlags() & (Box::NEED_EXPANSION | Box::NEED_CHILD_EXPANSION)) {
            if (child != getTableBox()) {
                assert(child->getNode());
                constructTablePart(child->getNode());
            } else {
                for (unsigned y = 0; y < yHeight; ++y) {
                    for (unsigned x = 0; x < xWidth; ++x) {
                        CellBoxPtr cellBox = grid[y][x];
                        if (!cellBox || cellBox->isSpanned(x, y))
                            continue;
                        if (cellBox->flags & (Box::NEED_EXPANSION | Box::NEED_CHILD_EXPANSION)) {
                            // TODO: deal with an anonymous cell
                            assert(cellBox->getNode());
                            if (cellBox->flags & Box::NEED_EXPANSION)
                                cellBox->resetWidth();
                            constructTablePart(cellBox->getNode());
                        }
                    }
                }
            }
            child->clearFlags(Box::NEED_EXPANSION | Box::NEED_CHILD_EXPANSION); // TODO: Refine
        }
    }
    flags &= ~(Box::NEED_EXPANSION | Box::NEED_CHILD_EXPANSION);
}

bool TableWrapperBox::processTableChild(Node node, const CSSStyleDeclarationPtr& style)
{
    unsigned display = CSSDisplayValueImp::None;
    Element child;
    CSSStyleDeclarationPtr childStyle;
    switch (node.getNodeType()) {
    case Node::TEXT_NODE:
        if (anonymousCell) {
            childStyle = style;
            display = CSSDisplayValueImp::Inline;
        } else {
            std::u16string data = interface_cast<Text>(node).getData();
            if (style) {
                char16_t c = ' ';
                size_t len = style->processWhiteSpace(data, c);
                if (0 < len) {
                    childStyle = style;
                    display = CSSDisplayValueImp::Inline;
                }
            } else {
                for (size_t i = 0; i < data.length(); ++i) {
                    if (!isSpace(data[i])) {
                        childStyle = style;
                        display = CSSDisplayValueImp::Inline;
                        break;
                    }
                }
            }
            if (display == CSSDisplayValueImp::Inline && isAnonymousTableObject())
                return false;
        }
        break;
    case Node::ELEMENT_NODE:
        child = interface_cast<Element>(node);
        childStyle = view->getStyle(child);
        if (childStyle)
            display = childStyle->display.getValue();
        break;
    default:
        return false;
    }

    switch (display) {
    case CSSDisplayValueImp::None:
        return true;
    case CSSDisplayValueImp::TableCaption:
        // 'table-caption' doesn't seem to end the current row:
        // cf. table-caption-003.
        if (BlockPtr caption = constructTablePart(child)) {
            if (childStyle->captionSide.getValue() == CSSCaptionSideValueImp::Top)
                topCaptions.push_back(caption);
            else
                bottomCaptions.push_back(caption);
        }
        break;
    case CSSDisplayValueImp::TableColumnGroup:
        endRow();
        endRowGroup();
        processColGroup(child);
        break;
    case CSSDisplayValueImp::TableColumn:
        endRow();
        processCol(child, childStyle, nullptr);
        break;
    case CSSDisplayValueImp::TableRow:
        endRow();
        // TODO
        processRow(child);
        break;
    case CSSDisplayValueImp::TableFooterGroup:
        endRow();
        if (pendingTfootElement)
            processRowGroup(child);
        else {
            pendingTfootElement = child;
            yTfootBegin = yHeight;
            processRowGroup(child);
            yTfootEnd = yHeight;
        }
        break;
    case CSSDisplayValueImp::TableHeaderGroup:
        endRow();
        if (pendingTheadElement)
            processRowGroup(child);
        else {
            pendingTheadElement = child;
            yTheadBegin = yHeight;
            processRowGroup(child);
            yTheadEnd = yHeight;
        }
        break;
    case CSSDisplayValueImp::TableRowGroup:
        endRow();
        processRowGroup(child);
        break;

    case CSSDisplayValueImp::TableCell:
        inRow = true;
        processCell(child, 0, childStyle, 0);
        break;
    default:
        inRow = true;
        if (!anonymousCell)
            anonymousCell = processCell(nullptr, 0, childStyle, 0);
        if (anonymousCell)
            view->constructBlock(node, anonymousCell, childStyle, 0);
        return true;
    }

    anonymousCell = 0;
    return true;
}

void TableWrapperBox::processRowGroup(Element section)
{
    CSSStyleDeclarationPtr sectionStyle = view->getStyle(section);

    for (Node node = section.getFirstChild(); node; node = node.getNextSibling())
        processRowGroupChild(node, sectionStyle);
    // TODO 3.
    endRowGroup();
}

void TableWrapperBox::processRowGroupChild(Node node, const CSSStyleDeclarationPtr& sectionStyle)
{
    unsigned display = CSSDisplayValueImp::None;
    Element child;
    CSSStyleDeclarationPtr childStyle;
    switch (node.getNodeType()) {
    case Node::TEXT_NODE:
        if (anonymousCell) {
            childStyle = sectionStyle;
            display = CSSDisplayValueImp::Inline;
        } else {
            std::u16string data = interface_cast<Text>(node).getData();
            if (sectionStyle) {
                char16_t c = ' ';
                size_t len = sectionStyle->processWhiteSpace(data, c);
                if (0 < len) {
                    childStyle = sectionStyle;
                    display = CSSDisplayValueImp::Inline;
                }
            } else {
                for (size_t i = 0; i < data.length(); ++i) {
                    if (!isSpace(data[i])) {
                        childStyle = sectionStyle;
                        display = CSSDisplayValueImp::Inline;
                        break;
                    }
                }
            }
        }
        break;
    case Node::ELEMENT_NODE:
        child = interface_cast<Element>(node);
        childStyle = view->getStyle(child);
        if (childStyle)
            display = childStyle->display.getValue();
        break;
    default:
        return;
    }

#if 0  // HTML talble model
    if (display != CSSDisplayValueImp::TableRow)
        return;
#endif

    unsigned yStart = yCurrent;
    switch (display) {
    case CSSDisplayValueImp::None:
        return;
    case CSSDisplayValueImp::TableRow:
        endRow();
        yStart = yCurrent;
        processRow(child);
        break;

    case CSSDisplayValueImp::TableCell:
        if (anonymousTable) {
            anonymousTable->constructBlocks();
            anonymousTable = 0;
        }
        inRow = true;
        processCell(child, 0, childStyle, nullptr);
        break;
    default:
        if (anonymousTable) {
            if (CSSDisplayValueImp::isTableParts(display)) {
                anonymousTable->processTableChild(node, childStyle);
                return;
            }
            anonymousTable->constructBlocks();
            anonymousTable = 0;
        }
        inRow = true;
        if (!anonymousCell)
            anonymousCell = processCell(nullptr, 0, childStyle, nullptr);
        if (anonymousCell) {
            anonymousTable = std::dynamic_pointer_cast<TableWrapperBox>(view->constructBlock(node, anonymousCell, childStyle, 0));
            if (display == CSSDisplayValueImp::Table || display == CSSDisplayValueImp::InlineTable)
                anonymousTable = 0;
        }
        break;
    }
    while (yStart < yHeight) {
        rowGroups[yStart] = sectionStyle;
        ++yStart;
    }
}

void TableWrapperBox::endRowGroup()
{
    endRow();
    while (yCurrent < yHeight) {
        growDownwardGrowingCells();
        ++yCurrent;
    }
    // downwardGrowingCells.clear();
}

void TableWrapperBox::processRow(Element row)
{
    CSSStyleDeclarationPtr rowStyle = view->getStyle(row);

    if (yHeight == yCurrent)
        appendRow();
    rows[yCurrent] = rowStyle;
    inRow = true;
    xCurrent = 0;
    growDownwardGrowingCells();
    for (Node node = row.getFirstChild(); node; node = node.getNextSibling())
        processRowChild(node, rowStyle);
    endRow();
}

void TableWrapperBox::growDownwardGrowingCells()
{
}

void TableWrapperBox::processRowChild(Node node, const CSSStyleDeclarationPtr& rowStyle)
{
    unsigned display = CSSDisplayValueImp::None;
    Element child;
    CSSStyleDeclarationPtr childStyle;
    switch (node.getNodeType()) {
    case Node::TEXT_NODE:
        if (anonymousCell) {
            childStyle = rowStyle;
            display = CSSDisplayValueImp::Inline;
        } else {
            std::u16string data = interface_cast<Text>(node).getData();
            if (rowStyle) {
                char16_t c = ' ';
                size_t len = rowStyle->processWhiteSpace(data, c);
                if (0 < len) {
                    childStyle = rowStyle;
                    display = CSSDisplayValueImp::Inline;
                }
            } else {
                for (size_t i = 0; i < data.length(); ++i) {
                    if (!isSpace(data[i])) {
                        childStyle = rowStyle;
                        display = CSSDisplayValueImp::Inline;
                        break;
                    }
                }
            }
        }
        break;
    case Node::ELEMENT_NODE:
        child = interface_cast<Element>(node);
        childStyle = view->getStyle(child);
        if (childStyle)
            display = childStyle->display.getValue();
        break;
    default:
        return;
    }

#if 0
    // HTML table model
    if (display != CSSDisplayValueImp::TableCell)
        continue;
#endif

    switch (display) {
    case CSSDisplayValueImp::None:
        return;
    case CSSDisplayValueImp::TableCell:
        processCell(child, 0, childStyle, rowStyle);
        break;
    default:
        if (anonymousTable) {
            if (CSSDisplayValueImp::isTableParts(display)) {
                anonymousTable->processTableChild(node, childStyle);
                return;
            }
            anonymousTable->constructBlocks();
            anonymousTable = 0;
        }
        if (!anonymousCell)
            anonymousCell = processCell(nullptr, 0, childStyle, rowStyle);
        if (anonymousCell) {
            anonymousTable = std::dynamic_pointer_cast<TableWrapperBox>(view->constructBlock(node, anonymousCell, childStyle, 0));
            if (display == CSSDisplayValueImp::Table || display == CSSDisplayValueImp::InlineTable)
                anonymousTable = 0;
        }
        return;
    }
    if (anonymousTable) {
        anonymousTable->constructBlocks();
        anonymousTable = 0;
    }
    anonymousCell = 0;
}

void TableWrapperBox::endRow()
{
    if (inRow) {
        inRow = false;
        ++yCurrent;
        if (anonymousTable) {
            anonymousTable->constructBlocks();
            anonymousTable = 0;
        }
        anonymousCell = 0;
    }
}

CellBoxPtr TableWrapperBox::processCell(Element current, const BlockPtr& parentBox, const CSSStyleDeclarationPtr& currentStyle, const CSSStyleDeclarationPtr& rowStyle)
{
    if (yHeight == yCurrent) {
        appendRow();
        xCurrent = 0;
    }
    rows[yCurrent] = rowStyle;
    while (xCurrent < xWidth && grid[yCurrent][xCurrent])
        ++xCurrent;
    if (xCurrent == xWidth)
        appendColumn();
    unsigned colspan = 1;
    unsigned rowspan = 1;
    if (html::HTMLTableCellElement::hasInstance(current)) {
        html::HTMLTableCellElement cell(interface_cast<html::HTMLTableCellElement>(current));
        colspan = cell.getColSpan();
        rowspan = cell.getRowSpan();
    }
    // TODO: 10 ?
    bool cellGrowsDownward = false;
    while (xWidth < xCurrent + colspan)
        appendColumn();
    while (yHeight < yCurrent + rowspan)
        appendRow();
    CellBoxPtr cellBox;
    if (current)
        cellBox = std::static_pointer_cast<CellBox>(constructTablePart(current));
    else {
        cellBox = std::make_shared<CellBox>();  // TODO: use parentStyle? -- For verticalAlign, yes.
        if (cellBox) {
            cellBox->stackingContext = stackingContext;
            cellBox->establishFormattingContext();
        }
    }
    if (cellBox) {
        cellBox->setPosition(xCurrent, yCurrent);
        cellBox->setColSpan(colspan);
        cellBox->setRowSpan(rowspan);
        for (unsigned x = xCurrent; x < xCurrent + colspan; ++x) {
            for (unsigned y = yCurrent; y < yCurrent + rowspan; ++y)
                grid[y][x] = cellBox;
        }
        // TODO: 13
        if (cellGrowsDownward)
            ;  // TODO: 14
        xCurrent += colspan;
    }
    return cellBox;
}

void TableWrapperBox::processColGroup(Element colgroup)
{
    bool hasCol = false;
    for (Element child = colgroup.getFirstElementChild(); child; child = child.getNextElementSibling()) {
        CSSStyleDeclarationPtr childStyle = view->getStyle(child);
        assert(childStyle);
        if (childStyle->display.getValue() != CSSDisplayValueImp::TableColumn)
            continue;
        hasCol = true;
        processCol(child, childStyle, colgroup);
    }
    if (hasCol) {
        // TODO. 7.
    } else {
        unsigned int span = 1;
        if (html::HTMLTableColElement::hasInstance(colgroup)) {
            html::HTMLTableColElement cg(interface_cast<html::HTMLTableColElement>(colgroup));
            span = cg.getSpan();
        }
        while (0 < span--) {
            appendColumn();
            columnGroups[xWidth - 1] = view->getStyle(colgroup);
        }
        // TODO 3.
    }
}

void TableWrapperBox::processCol(Element col, const CSSStyleDeclarationPtr& colStyle, Element colgroup)
{
    unsigned span = 1;
    if (html::HTMLTableColElement::hasInstance(col)) {
        html::HTMLTableColElement c(interface_cast<html::HTMLTableColElement>(col));
        span = c.getSpan();
    }
    while (0 < span--) {
        appendColumn();
        columns[xWidth - 1] = colStyle;
        columnGroups[xWidth - 1] = view->getStyle(colgroup);
    }
    // TODO 5.
}

void TableWrapperBox::processHeader()
{
    if (yTheadBegin == 0)
        return;
    unsigned headerCount = yTheadEnd - yTheadBegin;
    if (headerCount == 0)
        return;

    std::rotate(grid.begin(), grid.begin() + yTheadBegin, grid.begin() + yTheadEnd);
    std::rotate(rows.begin(), rows.begin() + yTheadBegin, rows.begin() + yTheadEnd);
    std::rotate(rowGroups.begin(), rowGroups.begin() + yTheadBegin, rowGroups.begin() + yTheadEnd);
    for (unsigned y = 0; y < headerCount; ++y) {
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y + yTheadBegin))
                continue;
            cellBox->row -= yTheadBegin;
        }
    }
    for (unsigned y = headerCount; y < yTheadEnd; ++y) {
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y - headerCount))
                continue;
            cellBox->row += headerCount;
        }
    }

    if (yTfootEnd <= yTheadBegin) {
        yTfootBegin += headerCount;
        yTfootEnd += headerCount;
    }
}

void TableWrapperBox::processFooter()
{
    if (yTfootEnd == yHeight)
        return;
    unsigned headerCount = yTfootEnd - yTfootBegin;
    if (headerCount == 0)
        return;

    unsigned offset = yHeight - yTfootEnd;
    for (unsigned y = yTfootBegin; y < yTfootEnd; ++y) {
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y))
                continue;
            cellBox->row += offset;
        }
    }
    for (unsigned y = yTfootEnd; y < yHeight; ++y) {
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y))
                continue;
            cellBox->row -= headerCount;
        }
    }
    std::rotate(grid.begin() + yTfootBegin, grid.begin() + yTfootEnd, grid.end());
    std::rotate(rows.begin() + yTfootBegin, rows.begin() + yTfootEnd, rows.end());
    std::rotate(rowGroups.begin() + yTfootBegin, rowGroups.begin() + yTfootEnd, rowGroups.end());
}

//
// Common part
//

void TableWrapperBox::fit(float w, FormattingContext* context)
{
    float diff = w - getTotalWidth();
    if (0.0f < diff) {
        unsigned autoMask = Left | Right;
        if (style) {
            if (!style->marginLeft.isAuto())
                autoMask &= ~Left;
            if (!style->marginRight.isAuto())
                autoMask &= ~Right;
        }
        switch (autoMask) {
        case Left | Right:
            diff /= 2.0f;
            marginLeft += diff;
            marginRight += diff;
            break;
        case Left:
            marginLeft += diff;
            break;
        case Right:
            marginRight += diff;
            break;
        default:
            break;
        }
    }
}

bool TableWrapperBox::
BorderValue::resolveBorderConflict(CSSBorderColorValueImp& c, CSSBorderStyleValueImp& s, CSSBorderWidthValueImp& w)
{
    if (s.getValue() == CSSBorderStyleValueImp::None || style.getValue() == CSSBorderStyleValueImp::Hidden)
        return false;
    if (s.getValue() == CSSBorderStyleValueImp::Hidden) {
        style.setValue(CSSBorderStyleValueImp::Hidden);
        width = 0.0f;
        return true;
    }
    float px = w.getPx();
    if (width < px || width == px && style < s) {
        color.specify(c);
        style.specify(s);
        width = px;
        return true;
    }
    return false;
}

void TableWrapperBox::
BorderValue::resolveBorderConflict(CSSStyleDeclarationPtr style, unsigned trbl)
{
    if (!style)
        return;
    if (trbl & 0x1)
        resolveBorderConflict(style->borderTopColor, style->borderTopStyle, style->borderTopWidth);
    if (trbl & 0x2)
        resolveBorderConflict(style->borderRightColor, style->borderRightStyle, style->borderRightWidth);
    if (trbl & 0x4)
        resolveBorderConflict(style->borderBottomColor, style->borderBottomStyle, style->borderBottomWidth);
    if (trbl & 0x8)
        resolveBorderConflict(style->borderLeftColor, style->borderLeftStyle, style->borderLeftWidth);
}

void TableWrapperBox::resolveHorizontalBorderConflict(unsigned x, unsigned y, BorderValue* b, const CellBoxPtr& top, const CellBoxPtr& bottom)
{
    if (top != bottom) {
        unsigned mask;
        if (y == 0)
            mask = 0x1;
        else if (y == yHeight)
            mask = 0x4;
        else
            mask = 0;
        if (bottom)
            b->resolveBorderConflict(bottom->getStyle(), 0x4);
        if (top)
            b->resolveBorderConflict(top->getStyle(), 0x1);
        if (0 < y)
            b->resolveBorderConflict(rows[y - 1], 0x4);
        if (y < yHeight)
            b->resolveBorderConflict(rows[y], 0x1);
        if (0 < y) {
            if (CSSStyleDeclarationPtr rowStyle = rowGroups[y - 1]) {
                if (y == yHeight || rowStyle != rowGroups[y])
                    b->resolveBorderConflict(rowStyle , 0x4);
            }
        }
        if (y < yHeight) {
            if (CSSStyleDeclarationPtr rowStyle = rowGroups[y]) {
                if (y == 0 || rowStyle != rowGroups[y - 1])
                    b->resolveBorderConflict(rowStyle, 0x1);
            }
        }
        if (mask) {
            b->resolveBorderConflict(columns[x], 0x5 & mask);
            b->resolveBorderConflict(columnGroups[x], 0x05 & mask);
            b->resolveBorderConflict(style, 0x05 & mask);
        }
    }
}

void TableWrapperBox::resolveVerticalBorderConflict(unsigned x, unsigned y, BorderValue* b, const CellBoxPtr& left, const CellBoxPtr& right)
{
    if (left != right) {
        unsigned mask;
        if (x == 0)
            mask = 0x8;
        else if (x == xWidth)
            mask = 0x2;
        else
            mask = 0;
        if (right)
            b->resolveBorderConflict(right->getStyle(), 0x2);
        if (left)
            b->resolveBorderConflict(left->getStyle(), 0x8);
        if (mask) {
            b->resolveBorderConflict(rows[y], 0xa & mask);
            b->resolveBorderConflict(rowGroups[y], 0xa & mask);
        }
        if (0 < x)
            b->resolveBorderConflict(columns[x - 1], 0x2);
        if (x < xWidth)
            b->resolveBorderConflict(columns[x], 0x8);
        if (0 < x) {
            if (CSSStyleDeclarationPtr colStyle = columnGroups[x - 1]) {
                if (x == xWidth || colStyle != columnGroups[x])
                    b->resolveBorderConflict(colStyle, 0x2);
            }
        }
        if (x < xWidth) {
            if (CSSStyleDeclarationPtr colStyle = columnGroups[x]) {
                if (x == 0 || colStyle != columnGroups[x - 1])
                    b->resolveBorderConflict(colStyle, 0x8);
            }
        }
        if (mask)
            b->resolveBorderConflict(style, 0x0a & mask);
    }
}

bool TableWrapperBox::resolveBorderConflict()
{
    assert(style);
    borderRows.clear();
    borderColumns.clear();
    if (style->borderCollapse.getValue() != CSSBorderCollapseValueImp::Collapse)
        return false;
    if (!getTableBox())
        return false;
    borderRows.resize((yHeight + 1) * xWidth);
    borderColumns.resize(yHeight * (xWidth + 1));
    for (unsigned y = 0; y < yHeight + 1; ++y) {
        for (unsigned x = 0; x < xWidth + 1; ++x) {
            if (x < xWidth) {
                BorderValue* br = getRowBorderValue(x, y);
                CellBoxPtr top = (y < yHeight) ? grid[y][x] : 0;
                CellBoxPtr bottom = (0 < y) ? grid[y - 1][x] : 0;
                resolveHorizontalBorderConflict(x, y, br, top, bottom);
            }
            if (y < yHeight) {
                BorderValue* bc = getColumnBorderValue(x, y);
                CellBoxPtr left = (x < xWidth) ? grid[y][x] : 0;
                CellBoxPtr right = (0 < x) ? grid[y][x - 1] : 0;
                resolveVerticalBorderConflict(x, y, bc, left, right);
            }
        }
    }
    return true;
}

void TableWrapperBox::computeTableBorders()
{
    float t = 0.0f;
    float b = 0.0f;
    for (unsigned x = 0; x < xWidth; ++x) {
        t = std::max(t, getRowBorderValue(x, 0)->getWidth() / 2.0f);
        b = std::max(b, getRowBorderValue(x, yHeight)->getWidth() / 2.0f);
    }
    float l = 0.0f;
    float r = 0.0f;
    if (0 < xWidth && 0 < yHeight) {
        l = getColumnBorderValue(0, 0)->getWidth() / 2.0f;
        r = getColumnBorderValue(xWidth, 0)->getWidth() / 2.0f;
    }
    BlockPtr tableBox(getTableBox());
    assert(tableBox);
    tableBox->expandBorders(t, r, b, l);
    // Note in 17.6.2, the spec says 'any excess spills into the margin
    // area of the table'. However, it can actually overflow that table's
    // container. Therefore, the code like below is not necessary:
    // cf. collapsing-border-model-005.
    //
    // for (unsigned y = 0; y < yHeight; ++y) {
    //     float excess;
    //     excess = std::max(borderLeft, getColumnBorderValue(0, y)->getWidth() / 2.0f - l) - borderLeft;
    //     marginLeft = std::max(marginLeft, excess);
    //     excess = std::max(borderRight, getColumnBorderValue(xWidth, y)->getWidth() / 2.0f - r) - borderRight;
    //     marginRight = std::max(marginRight, excess);
    // }
}

void TableWrapperBox::layOutFixed(ViewCSSImp* view, const ContainingBlockPtr& containingBlock, bool collapsingModel)
{
    if (xWidth == 0 || yHeight == 0)
        return;
    float hs = 0.0f;
    if (!collapsingModel) {
        hs = style->borderSpacing.getHorizontalSpacing();
        BlockPtr tableBox(getTableBox());
        assert(tableBox);
        tableBox->width -= tableBox->getBorderWidth() - tableBox->width;  // TODO: HTML, XHTML only
        if (tableBox->width < 0.0f)
            tableBox->width = 0.0f;
    }
    float sum = hs;
    unsigned remainingColumns = xWidth;
    for (unsigned x = 0; x < xWidth; ++x) {
        if (CSSStyleDeclarationPtr colStyle = columns[x]) {
            if (!colStyle->width.isAuto()) {
                colStyle->resolve(view, containingBlock);
                widths[x] = colStyle->borderLeftWidth.getPx() +
                            colStyle->paddingLeft.getPx() +
                            colStyle->width.getPx() +
                            colStyle->paddingRight.getPx() +
                            colStyle->borderRightWidth.getPx() +
                            hs;
                sum += widths[x];
                --remainingColumns;
                continue;
            }
        }
        CellBoxPtr cellBox = grid[0][x];
        if (!cellBox || cellBox->isSpanned(x, 0))
            continue;
        CSSStyleDeclarationPtr cellStyle = cellBox->getStyle();
        if (!cellStyle || cellStyle->width.isAuto())
            continue;
        cellStyle->resolve(view, containingBlock);
        unsigned span = cellBox->getColSpan();
        float w = cellStyle->borderLeftWidth.getPx() +
                  cellStyle->paddingLeft.getPx() +
                  cellStyle->width.getPx() +
                  cellStyle->paddingRight.getPx() +
                  cellStyle->borderRightWidth.getPx() +
                  hs * span;
        w /= span;
        for (unsigned i = x; i < x + span; ++i) {
            widths[i] = w;
            sum += w;
            --remainingColumns;
        }
    }
    if (0 < remainingColumns) {
        float w = std::max(0.0f, width - sum) / remainingColumns;
        for (unsigned x = 0; x < xWidth; ++x) {
            if (isnan(widths[x])) {
                widths[x] = w;
                sum += w;
            }
        }
    }
    if (width <= sum)
        width = sum;
    else {
        float w = (width - sum) / xWidth;
        for (unsigned x = 0; x < xWidth; ++x)
            widths[x] += w;
    }
    widths[0] += hs / 2.0f;
    widths[xWidth - 1] += hs / 2.0f;
}

void TableWrapperBox::layOutAuto(ViewCSSImp* view, const ContainingBlockPtr& containingBlock)
{
    if (xWidth == 0 || yHeight == 0)
        return;
    for (unsigned x = 0; x < xWidth; ++x) {
        if (CSSStyleDeclarationPtr colStyle = columns[x]) {
            if (!colStyle->width.isAuto()) {
                colStyle->resolve(view, containingBlock);
                widths[x] = colStyle->borderLeftWidth.getPx() +
                            colStyle->paddingLeft.getPx() +
                            colStyle->paddingRight.getPx() +
                            colStyle->borderRightWidth.getPx();
                if (!colStyle->width.isPercentage())
                    widths[x] += colStyle->width.getPx();
                else
                    percentages[x] = colStyle->width.getPercentage();
            }
        }
    }
}

void TableWrapperBox::layOutAutoColgroup(ViewCSSImp* view, const ContainingBlockPtr& containingBlock)
{
    if (xWidth == 0 || yHeight == 0)
        return;
    for (unsigned x = 0; x < xWidth; ++x) {
        CSSStyleDeclarationPtr columnGroupStyle = columnGroups[x];
        if (!columnGroupStyle)
            continue;
        size_t elements = 1;
        float sum = widths[x];
        while (columnGroupStyle == columnGroups[x + elements]) {
            ++elements;
            sum += widths[x + elements];
        }
        if (!columnGroupStyle->width.isAuto()) {
            columnGroupStyle->resolve(view, containingBlock);
            float w = columnGroupStyle->borderLeftWidth.getPx() +
                      columnGroupStyle->paddingLeft.getPx() +
                      columnGroupStyle->paddingRight.getPx() +
                      columnGroupStyle->borderRightWidth.getPx();
            if (!columnGroupStyle->width.isPercentage())
                w += columnGroupStyle->width.getPx();
            if (sum < w) {
                float diff = (w - sum) / elements;
                for (unsigned c = 0; c < elements; ++c)
                    widths[x + c] += diff;
            }
        }
        x += elements - 1;
    }
}

void TableWrapperBox::layOutTableBox(ViewCSSImp* view, FormattingContext* context, const ContainingBlockPtr& containingBlock, bool collapsingModel, bool fixedLayout)
{
    BlockPtr tableBox(getTableBox());
    if (!tableBox)
        return;

    tableBox->setStyle(style);
    tableBox->setPosition(CSSPositionValueImp::Static);
    bool anon = style->display != CSSDisplayValueImp::Table && style->display != CSSDisplayValueImp::InlineTable;
    float hs = 0.0f;
    tableBox->backgroundColor = 0x00000000;
    // TODO: process backgroundImage
    tableBox->paddingTop = tableBox->paddingRight = tableBox->paddingBottom = tableBox->paddingLeft = 0.0f;
    tableBox->borderTop = tableBox->borderRight = tableBox->borderBottom = tableBox->borderLeft = 0.0f;
    if (!anon) {
        tableBox->resolveBackground(view);
        if (!collapsingModel) {
            tableBox->updatePadding();
            tableBox->updateBorderWidth();
            hs = style->borderSpacing.getHorizontalSpacing();
        }
    }
    // Note 'width' needs to be preserved since it is used as the width of
    // the containing block of cells.
    tableBox->width = width;
    if (style->width.isAuto()) {    // cf. html4/separated-border-model-004.htm
        tableBox->width -= tableBox->getBlankLeft() + tableBox->getBlankRight();
        if (tableBox->width < 0.0f)
            tableBox->width = 0.0f;
    }
    tableBox->height = height;
    if (anon || style->height.isAuto())
        height = tableBox->height = 0.0f;
    widths.resize(xWidth);
    fixedWidths.resize(xWidth);
    percentages.resize(xWidth);
    heights.resize(yHeight);
    baselines.resize(yHeight);

    rowImages.resize(yHeight);
    for (unsigned y = 0; y < yHeight; ++y) {
        rowImages[y] = 0;
        CSSStyleDeclarationPtr rowStyle = rows[y];
        if (rowStyle && !rowStyle->backgroundImage.isNone()) {
            HttpRequestPtr request = view->preload(view->getDocument()->getDocumentURI(), rowStyle->backgroundImage.getValue());
            if (request)
                rowImages[y] = request->getBoxImage(rowStyle->backgroundRepeat.getValue());
        }
    }
    rowGroupImages.resize(yHeight);
    for (unsigned y = 0; y < yHeight; ++y) {
        rowGroupImages[y] = 0;
        CSSStyleDeclarationPtr rowGroupStyle = rowGroups[y];
        if (rowGroupStyle) {
            if (!rowGroupStyle->backgroundImage.isNone()) {
                HttpRequestPtr request = view->preload(view->getDocument()->getDocumentURI(), rowGroupStyle->backgroundImage.getValue());
                if (request)
                    rowGroupImages[y] = request->getBoxImage(rowGroupStyle->backgroundRepeat.getValue());
            }
            while (rowGroupStyle == rowGroups[++y])
                ;
            --y;
        }
    }

    columnImages.resize(xWidth);
    for (unsigned x = 0; x < xWidth; ++x) {
        columnImages[x] = 0;
        CSSStyleDeclarationPtr columnStyle = columns[x];
        if (columnStyle && !columnStyle->backgroundImage.isNone()) {
            HttpRequestPtr request = view->preload(view->getDocument()->getDocumentURI(), columnStyle->backgroundImage.getValue());
            if (request)
                columnImages[x] = request->getBoxImage(columnStyle->backgroundRepeat.getValue());
        }
    }
    columnGroupImages.resize(xWidth);
    for (unsigned x = 0; x < xWidth; ++x) {
        columnGroupImages[x] = 0;
        CSSStyleDeclarationPtr columnGroupStyle = columnGroups[x];
        if (columnGroupStyle) {
            if (!columnGroupStyle->backgroundImage.isNone()) {
                HttpRequestPtr request = view->preload(view->getDocument()->getDocumentURI(), columnGroupStyle->backgroundImage.getValue());
                if (request)
                    columnGroupImages[x] = request->getBoxImage(columnGroupStyle->backgroundRepeat.getValue());
            }
            while (columnGroupStyle == columnGroups[++x])
                ;
            --x;
        }
    }

    for (unsigned x = 0; x < xWidth; ++x) {
        widths[x] = fixedWidths[x] = fixedLayout ? NAN : 0.0f;
        percentages[x] = -1.0f;
    }
    if (fixedLayout)
        layOutFixed(view, containingBlock, collapsingModel);
    else
        layOutAuto(view, containingBlock);
    float tableWidth = width;
    int pass = 0;
Reflow:
    for (unsigned y = 0; y < yHeight; ++y) {
        heights[y] = baselines[y] = 0.0f;
        if (rows[y] && !rows[y]->height.isAuto())
            heights[y] = rows[y]->height.getPx();
        bool noBaseline = true;
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y))
                continue;
            if (fixedLayout) {
                cellBox->columnWidth = widths[x];
                for (unsigned i = x + 1; i < x + cellBox->getColSpan(); ++i)
                    cellBox->columnWidth += widths[i];
                tableBox->width += cellBox->columnWidth;
                tableBox->width -= hs;
                if (x == 0)
                    tableBox->width -= hs / 2.0f;
                if (x + cellBox->getColSpan() == xWidth)
                    tableBox->width -= hs / 2.0f;
                cellBox->fixedLayout = true;
            }
            cellBox->layOut(view, 0);
            cellBox->intrinsicHeight = cellBox->getTotalHeight();
            // Process 'height' as the minimum height.
            CSSStyleDeclarationPtr cellStyle = cellBox->isAnonymous() ? nullptr : cellBox->getStyle();
            if (cellBox->getRowSpan() == 1) {
                heights[y] = std::max(heights[y], cellBox->getTotalHeight());
                if (cellStyle && !cellStyle->height.isAuto())
                    heights[y] = std::max(heights[y], cellStyle->height.getPx() + cellBox->getBlankTop() + cellBox->getBlankBottom());
            }
            if (!fixedLayout && cellBox->getColSpan() == 1) {
                if (cellStyle) {
                    if (cellStyle->width.isPercentage()) {
                        percentages[x] = std::max(percentages[x], cellStyle->width.getPercentage());
                        fixedWidths[x] = std::max(fixedWidths[x], ceilf(cellBox->getTotalWidth()));
                    } else if (!cellStyle->width.isAuto())
                        fixedWidths[x] = std::max(fixedWidths[x], cellBox->getMCW());
                    else
                        fixedWidths[x] = std::max(fixedWidths[x], ceilf(cellBox->getTotalWidth()));
                } else
                    fixedWidths[x] = std::max(fixedWidths[x], ceilf(cellBox->getTotalWidth()));
                widths[x] = std::max(widths[x], cellBox->getMCW());
            }
            if (cellBox->getVerticalAlign() == CSSVerticalAlignValueImp::Baseline) {
                baselines[y] = std::max(baselines[y], cellBox->getBaseline());
                noBaseline = false;
            }
        }
        // Process baseline
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y) || cellBox->getRowSpan() != 1)
                continue;
            if (cellBox->getVerticalAlign() == CSSVerticalAlignValueImp::Baseline)
                heights[y] = std::max(heights[y], (baselines[y] - cellBox->getBaseline()) + cellBox->intrinsicHeight);
            else if (noBaseline) {
                switch (cellBox->getVerticalAlign()) {
                case CSSVerticalAlignValueImp::Top:
                    baselines[y] = std::max(baselines[y], cellBox->intrinsicHeight - cellBox->getBlankBottom());
                    break;
                case CSSVerticalAlignValueImp::Bottom:
                    baselines[y] = std::max(baselines[y], heights[y] - cellBox->getBlankBottom());
                    break;
                case CSSVerticalAlignValueImp::Middle:
                    baselines[y] = std::max(baselines[y], (heights[y] + cellBox->intrinsicHeight) / 2.0f - cellBox->getBlankBottom());
                    break;
                default:
                    break;
                }
            }
        }
    }
    if (fixedLayout)
        tableBox->width = tableWidth;
    for (unsigned x = 0; x < xWidth; ++x) {
        for (unsigned y = 0; y < yHeight; ++y) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y))
                continue;
            if (!fixedLayout) {
                unsigned span = cellBox->getColSpan();
                if (1 < span) {
                    float sum = 0.0f;
                    for (unsigned c = 0; c < span; ++c)
                        sum += widths[x + c];
                    if (sum < cellBox->getMCW()) {
                        float diff = (cellBox->getMCW() - sum) / span;
                        for (unsigned c = 0; c < span; ++c)
                            widths[x + c] += diff;
                    }
                    sum = 0.0f;
                    for (unsigned c = 0; c < span; ++c)
                        sum += fixedWidths[x + c];
                    if (sum < cellBox->getTotalWidth()) {
                        float diff = (cellBox->getTotalWidth() - sum) / span;
                        for (unsigned c = 0; c < span; ++c)
                            fixedWidths[x + c] += diff;
                    }
                }
            }
            unsigned span = cellBox->getRowSpan();
            if (1 < span) {
                float sum = 0.0f;
                for (unsigned r = 0; r < span; ++r)
                    sum += heights[y + r];

                CSSStyleDeclarationPtr cellStyle = cellBox->isAnonymous() ? nullptr : cellBox->getStyle();
                if (cellStyle && !cellStyle->height.isAuto()) {
                    float h = cellStyle->height.getPx() + cellBox->getBlankTop() + cellBox->getBlankBottom();
                    if (sum < h) {
                        float diff = (h - sum) / span;
                        for (unsigned r = 0; r < span; ++r)
                            heights[y + r] += diff;
                        sum = h;
                    }
                }

                float d = 0.0f;
                if (cellBox->getVerticalAlign() == CSSVerticalAlignValueImp::Baseline)
                    d = baselines[y] - cellBox->getBaseline();
                if (sum < d + cellBox->intrinsicHeight) {
                    float diff = (d + cellBox->intrinsicHeight - sum) / span;
                    for (unsigned r = 0; r < span; ++r)
                        heights[y + r] += diff;
                }
            }
        }
    }
    if (!fixedLayout) {
        layOutAutoColgroup(view, containingBlock);
        // At this point, MCWs of cells have been calculated.
        float w = 0.0f;
        float p = 0.0f;
        for (unsigned x = 0; x < xWidth; ++x) {
            w += widths[x];
            p += std::max(fixedWidths[x], widths[x]);
        }
        if (anon || style->width.isAuto()) {
            if (containingBlock->width <= w)
                tableBox->width = w;
            else if (containingBlock->width < p)
                tableBox->width = containingBlock->width;
            else {
                tableBox->width = w = p;
                for (unsigned x = 0; x < xWidth; ++x)
                    widths[x] = std::max(fixedWidths[x], widths[x]);
            }
        }
        if (w < tableBox->width) {
            float r = tableBox->width - w;
            unsigned fixed = 0;
            for (unsigned x = 0; x < xWidth; ++x) {
                if (fixedWidths[x] <= widths[x])
                    ++fixed;
                else if (0.0f <= percentages[x]) {
                    ++fixed;
                    float cw = tableBox->width * percentages[x] / 100.0f;
                    cw -= widths[x];
                    if (0.0f < cw) {
                        cw = std::min(cw, r);
                        r -= cw;
                        widths[x] += cw;
                    }
                }
            }
            while (fixed < xWidth && 0.0f < r) {
                w = r / (xWidth - fixed);
                r = 0.0f;
                for (unsigned x = 0; x < xWidth; ++x) {
                    if (percentages[x] < 0.0f && widths[x] < fixedWidths[x]) {
                        float d = fixedWidths[x] - widths[x];
                        if (w < d)
                            widths[x] += w;
                        else {
                            ++fixed;
                            widths[x] = fixedWidths[x];
                            r += w - d;
                        }
                    }
                }
            }
            if (0.0f < r) {
                fixed = 0;
                for (unsigned x = 0; x < xWidth; ++x) {
                    if (widths[x] <= 0.0f)
                        ++fixed;
                }
                if (fixed < xWidth) {
                    w = r / (xWidth - fixed);
                    r = 0.0f;
                    for (unsigned x = 0; x < xWidth; ++x) {
                        if (0.0f < widths[x])
                            widths[x] += w;
                    }
                }
            }
            if (0.0f < r) {
                w = r / xWidth;
                r = 0.0f;
                for (unsigned x = 0; x < xWidth; ++x)
                    widths[x] += w;
            }
        }
        if (++pass == 1) {
            // At this point, column widths have been determined.
            for (unsigned x = 0; x < xWidth; ++x) {
                for (unsigned y = 0; y < yHeight; ++y) {
                    CellBoxPtr cellBox = grid[y][x];
                    if (!cellBox || cellBox->isSpanned(x, y))
                        continue;
                    unsigned span = cellBox->getColSpan();
                    if (span == 1)
                        cellBox->columnWidth = widths[x];
                    else {
                        float sum = 0.0f;
                        for (unsigned c = 0; c < span; ++c)
                            sum += widths[x + c];
                        cellBox->columnWidth = sum;
                    }
                }
            }
            // Reflow cells using the MCWs as the specified widths.
            goto Reflow;
        }
    }
    float h = 0.0f;
    for (unsigned y = 0; y < yHeight; ++y)
        h += heights[y];
    tableBox->height = std::max(tableBox->height, h);
    if (h < tableBox->height) {
        h = (tableBox->height - h) / yHeight;
        for (unsigned y = 0; y < yHeight; ++y)
            heights[y] += h;
    }

    for (unsigned x = 0; x < xWidth; ++x) {
        for (unsigned y = 0; y < yHeight; ++y) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y))
                continue;
            if (!fixedLayout) {
                float w = widths[x];
                for (unsigned c = 1; c < cellBox->getColSpan(); ++c)
                    w += widths[x + c];
                cellBox->fit(w, context);
            }
            float h = heights[y];
            for (unsigned r = 1; r < cellBox->getRowSpan(); ++r)
                h += heights[y + r];
            cellBox->height = h - cellBox->getBlankTop() - cellBox->getBlankBottom();
            // Align cellBox vertically
            float d = 0.0f;
            switch (cellBox->getVerticalAlign()) {
            case CSSVerticalAlignValueImp::Top:
                break;
            case CSSVerticalAlignValueImp::Bottom:
                d = cellBox->getTotalHeight() - cellBox->intrinsicHeight;
                break;
            case CSSVerticalAlignValueImp::Middle:
                d = (cellBox->getTotalHeight() - cellBox->intrinsicHeight) / 2.0f;
                break;
            default:
                d = baselines[y] - cellBox->getBaseline();
                break;
            }
            if (0.0f < d) {
                cellBox->paddingTop += d;
                cellBox->height -= d;
            }
        }
    }

    BoxPtr lineBox = tableBox->getFirstChild();
    for (unsigned y = 0; y < yHeight; ++y)  {
        assert(lineBox);
        lineBox->width = tableBox->width;
        lineBox->height = heights[y];
        lineBox = lineBox->getNextSibling();

        float xOffset = 0.0f;
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y) && cellBox->row != y) {
                xOffset += widths[x];
                continue;
            }
            if (!cellBox->isSpanned(x, y)) {
                cellBox->offsetH = xOffset; // Note using offsetH for adjusting column spans since cells are not positioned.
                if (CSSStyleDeclarationPtr style = cellBox->getStyle())
                    style->setContainingBlockSize(tableBox->width, tableBox->height);
            }
        }
    }

    if (collapsingModel)
        computeTableBorders();
    width = tableBox->getBlockWidth();
    tableBox->resolveBackgroundPosition(view, containingBlock);

    for (BoxPtr child = getFirstChild(); child; child = child->getNextSibling()) {
        if (child != tableBox) {
            child->layOut(view, context);
            width = std::max(width, child->getBlockWidth());
        }
    }

    mcw = width;
    if (!isAnonymous()) {
        if (!style->marginLeft.isPercentage() && !style->marginLeft.isAuto())
            mcw += style->marginLeft.getPx();
        if (!style->marginRight.isPercentage() && !style->marginRight.isAuto())
            mcw += style->marginRight.getPx();
    }

    h = 0.0f;
    for (BoxPtr child = getFirstChild(); child; child = child->getNextSibling())
        h += child->getTotalHeight() + child->getClearance();
    height = std::max(height, h);

    tableBox->flags &= ~(NEED_EXPANSION | NEED_REFLOW | NEED_CHILD_REFLOW);
}

void TableWrapperBox::layOutTableParts()
{
    for (auto i = blockMap.begin(); i != blockMap.end(); ++i) {
        BlockPtr block = i->second;
        if (!block->needLayout())
            continue;
        if (auto cellBox = std::dynamic_pointer_cast<CellBox>(block)) {
            float savedMCW = cellBox->getMCW();
            float savedWidth = cellBox->getTotalWidth();
            float savedIntrinsicHeight = cellBox->intrinsicHeight;
            float savedPadding = cellBox->paddingTop;
            float savedHeight = cellBox->height;
            // TODO: Check block's baseline as well.
            cellBox->layOut(view, 0);
            if (savedMCW != cellBox->getMCW() || savedWidth != cellBox->getTotalWidth() || savedIntrinsicHeight != cellBox->getTotalHeight())
                flags |= NEED_REFLOW;
            else {
                // Restore vertical alignment
                cellBox->paddingTop = savedPadding;
                cellBox->height = savedHeight;
            }
        } else {
            float savedWidth = block->getTotalWidth();
            float savedHeight = block->getTotalHeight();
            // TODO: Check block's baseline as well.
            block->layOut(view, 0);
            if (savedWidth != block->getTotalWidth() || savedHeight != block->getTotalHeight())
                flags |= NEED_REFLOW;
        }
    }
    auto tableBox(getTableBox());
    assert(tableBox);
    tableBox->flags &= ~NEED_CHILD_REFLOW;
}

void TableWrapperBox::resetCellBoxWidths()
{
    for (unsigned y = 0; y < yHeight; ++y) {
        for (unsigned x = 0; x < xWidth; ++x) {
            CellBoxPtr cellBox = grid[y][x];
            if (!cellBox || cellBox->isSpanned(x, y))
                continue;
            cellBox->resetWidth();
        }
    }
}

bool TableWrapperBox::layOut(ViewCSSImp* view, FormattingContext* context)
{
    FormattingContext* parentContext = context;

    ContainingBlockPtr containingBlock = getContainingBlock(view);
    style = view->getStyle(table);
    if (!style)
        return false;  // TODO error

    float savedWidth = width;
    float savedHeight = height;
    // TODO: Check anonymous table
    flags |= style->resolve(view, containingBlock);

    // The computed values of properties 'position', 'float', 'margin-*', 'top', 'right', 'bottom',
    // and 'left' on the table element are used on the table wrapper box and not the table box;
    backgroundColor = getParentBox() ? 0x00000000 : style->backgroundColor.getARGB();
    paddingTop = paddingRight = paddingBottom = paddingLeft = 0.0f;
    borderTop = borderRight = borderBottom = borderLeft = 0.0f;
    float leftover = resolveWidth(containingBlock->width, context);
    resolveHeight();

    visibility = style->visibility.getValue();
    textAlign = style->textAlign.getValue();

    if (context) {
        collapseMarginTop(context);
        if (!isnan(context->clearance))
            leftover = resolveWidth(containingBlock->width, context);
    }
    context = updateFormattingContext(context);

    if (isCollapsableOutside()) {
        context->inheritMarginContext(parentContext);
        // The top margin of the table caption is not collapsed with top margin of the table.
        // cf. html4/table-anonymous-block-011.htm
        context->fixMargin();
    }

    if (!(getFlags() & NEED_REFLOW)) {
        if (flags & NEED_CHILD_REFLOW)
            layOutTableParts();
        if (!(flags & NEED_REFLOW)) {
            // Only inline blocks have been marked NEED_REFLOW and no more reflow is necessary
            // with this block.
#ifndef NDEBUG
            if (3 <= getLogLevel())
                std::cout << "TableWrapperBox::" << __func__ << ": skip table reflow\n";
#endif
            flags &= ~(NEED_CHILD_REFLOW | NEED_REPOSITION);
            width = savedWidth;
            height = savedHeight;
            restoreFormattingContext(context);
            if (parentContext && context != parentContext)
                context = parentContext;
            if (context) {
                context->restoreContext(self());
                return true;
            }
        }
    }

    bool collapsingModel = resolveBorderConflict();
    bool fixedLayout = (style->tableLayout.getValue() == CSSTableLayoutValueImp::Fixed) && !style->width.isAuto();

    resetCellBoxWidths();
    layOutTableBox(view, context, containingBlock, collapsingModel, fixedLayout);

    restoreFormattingContext(context);
    if (parentContext && parentContext != context) {
        if (isCollapsableOutside()) {
            parentContext->inheritMarginContext(context);
            if (!isAnonymous() && isFlowRoot() && leftover < width) {
                float h;
                while (0 < (h = parentContext->shiftDown(&leftover))) {
                    parentContext->updateRemainingHeight(h);
                    if (isnan(clearance))
                        clearance = h;
                    else
                        clearance += h;
                    if (width <= leftover)
                        break;
                }
                if (!isnan(clearance)) {
                    // TODO: Adjust clearance and set margins to the original values
                    resolveWidth(containingBlock->width, parentContext, width);
                    updateFormattingContext(parentContext);
                }
            }
        }
        context = parentContext;
        if (0.0f < height)
            context->updateRemainingHeight(height);
        // TODO: if there's not enough width...
    }

    if (context->hasChanged(self())) {
        context->saveContext(self());
        if (nextSibling)
            nextSibling->setFlags(NEED_REFLOW);
    }
    flags &= ~(NEED_REFLOW | NEED_CHILD_REFLOW | NEED_REPOSITION);
    return true;
}

void TableWrapperBox::layOutAbsolute(ViewCSSImp* view)
{
    assert(isAbsolutelyPositioned());
    style = view->getStyle(table);
    if (!style)
        return;

    float savedWidth = width;
    float savedHeight = height;

    setContainingBlock(view);
    ContainingBlockPtr containingBlock = getContainingBlock(view);
    flags |= style->resolve(view, containingBlock);
    visibility = style->visibility.getValue();

    // The computed values of properties 'position', 'float', 'margin-*', 'top', 'right', 'bottom',
    // and 'left' on the table element are used on the table wrapper box and not the table box;
    backgroundColor = getParentBox() ? 0x00000000 : style->backgroundColor.getARGB();
    paddingTop = paddingRight = paddingBottom = paddingLeft = 0.0f;
    borderTop = borderRight = borderBottom = borderLeft = 0.0f;

    float left;
    float right;
    unsigned maskH = resolveAbsoluteWidth(containingBlock, left, right);
    applyAbsoluteMinMaxWidth(containingBlock, left, right, maskH);
    float top;
    float bottom;
    unsigned maskV = resolveAbsoluteHeight(containingBlock, top, bottom);
    applyAbsoluteMinMaxHeight(containingBlock, top, bottom, maskV);

    if (CSSDisplayValueImp::isBlockLevel(style->display.getOriginalValue())) {
        // This box is originally a block-level box inside an inline context.
        // Set the static position to the beginning of the next line.
        if (BoxPtr lineBox = getParentBox()) {  // A root element can be absolutely positioned.
            for (BoxPtr box = getPreviousSibling(); box; box = box->getPreviousSibling()) {
                if (!box->isAbsolutelyPositioned()) {
                    if (maskV == (Top | Height | Bottom) || maskV == (Top | Bottom))
                        offsetV += lineBox->height + lineBox->getBlankBottom();
                    if (maskH == (Left | Width | Right) || maskH == (Left | Right))
                        offsetH -= box->getTotalWidth();
                }
            }
        }
    }

    if (!(flags & NEED_REFLOW)) {
        if (flags & NEED_CHILD_REFLOW)
            layOutTableParts();
        if (!(flags & NEED_REFLOW)) {
            // Only inline blocks have been marked NEED_REFLOW and no more reflow is necessary
            // with this block.
            width = savedWidth;
            if (maskH & Left)
                left = containingBlock->width - getTotalWidth() - right;
            maskH = applyAbsoluteMinMaxWidth(containingBlock, left, right, maskH);

            float before = height;
            height = savedHeight;
            if (maskV == (Top | Height))
                top += before - height;
            maskV = applyAbsoluteMinMaxHeight(containingBlock, top, bottom, maskV);

#ifndef NDEBUG
            if (3 <= getLogLevel())
                std::cout << "Block::" << __func__ << ": skip table reflow\n";
#endif
            layOutAbsoluteEnd(left, top);
            return;
        }
    }

    bool collapsingModel = resolveBorderConflict();
    bool fixedLayout = (style->tableLayout.getValue() == CSSTableLayoutValueImp::Fixed) && !style->width.isAuto();

    FormattingContext* context = updateFormattingContext(0);
    assert(context);

    float before = height;
    resetCellBoxWidths();
    layOutTableBox(view, context, containingBlock, collapsingModel, fixedLayout);

    if (maskH == (Left | Width))
        left = containingBlock->width - getTotalWidth() - right;
    if (maskV == (Top | Height))
        top += before - height;

    restoreFormattingContext(context);
    adjustCollapsedThroughMargins(context);
    layOutAbsoluteEnd(left, top);
}

float TableWrapperBox::shrinkTo()
{
    float min = width;
    min += borderLeft + paddingLeft + paddingRight + borderRight;
    if (style) {
        if (!style->marginLeft.isAuto())
            min += style->marginLeft.getPx();
        if (!style->marginRight.isAuto()) {
            float m  = style->marginRight.getPx();
            if (0.0f < m)
                min += m;
        }
    }
    return min;
}

// cf. 10.8.1 - The baseline of an 'inline-table' is the baseline of the first row of the table.
float TableWrapperBox::getBaseline() const
{
    auto tableBox(getTableBox());
    assert(tableBox);
    float baseline = getBlankTop();
    for (BoxPtr child = getFirstChild(); child; child = child->getNextSibling()) {
        if (child == tableBox) {
            if (0 < yHeight) {
                baseline += tableBox->getBlankTop() + baselines[0];
                break;
            }
        }
        baseline += child->getTotalHeight() + child->getClearance();
    }
    return baseline;
}

void TableWrapperBox::dump(std::string indent)
{
    std::cout << indent << "* table wrapper box";
    if (!node)
        std::cout << " [anonymous";
    else
        std::cout << " [" << interface_cast<Element>(node).getLocalName();
    if (3 <= getLogLevel())
        std::cout << ':' << uid << '|' << std::hex << flags << std::dec << '(' << count_() << ')';
    std::cout << ']';
    std::cout << " (" << x << ", " << y << ") " <<
        "w:" << width << " h:" << height << ' ' <<
        "[" << xWidth << ", " << yHeight << "] " <<
        "(" << offsetH << ", " << offsetV <<") ";
    if (hasClearance())
        std::cout << "c:" << clearance << ' ';
    std::cout << "m:" << marginTop << ':' << marginRight << ':' << marginBottom << ':' << marginLeft << '\n';
    indent += "  ";
    for (BoxPtr child = getFirstChild(); child; child = child->getNextSibling())
        child->dump(indent);
}

bool Block::isTableBox() const
{
    if (auto wrapper = std::dynamic_pointer_cast<TableWrapperBox>(getParentBox())) {
        if (wrapper->isTableBox(self()))
            return true;
    }
    return false;
}

}}}}  // org::w3c::dom::bootstrap
