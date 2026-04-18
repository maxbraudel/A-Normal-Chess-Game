#include "Input/LayeredSelection.hpp"

#include "Board/Cell.hpp"

bool LayeredSelectionStack::isEmpty() const {
    return count == 0;
}

bool LayeredSelectionStack::contains(SelectionLayer layer) const {
    return indexOf(layer) >= 0;
}

int LayeredSelectionStack::indexOf(SelectionLayer layer) const {
    for (int index = 0; index < count; ++index) {
        if (layers[index] == layer) {
            return index;
        }
    }

    return -1;
}

SelectionLayer LayeredSelectionStack::top() const {
    return isEmpty() ? SelectionLayer::None : layers[0];
}

SelectionLayer LayeredSelectionStack::nextBelow(SelectionLayer currentLayer) const {
    if (isEmpty()) {
        return SelectionLayer::None;
    }

    const int currentIndex = indexOf(currentLayer);
    if (currentIndex < 0) {
        return top();
    }

    return layers[(currentIndex + 1) % count];
}

LayeredSelectionStack resolveCellSelectionStack(const Cell& cell, sf::Vector2i cellPos,
                                                Piece* pieceOverride,
                                                bool suppressCellPiece) {
    LayeredSelectionStack stack;
    stack.cellPos = cellPos;

    if (!cell.isInCircle) {
        return stack;
    }

    stack.piece = pieceOverride ? pieceOverride : (suppressCellPiece ? nullptr : cell.piece);
    stack.autonomousUnit = cell.autonomousUnit;
    stack.building = cell.building;
    stack.mapObject = cell.mapObject;
    stack.hasTerrain = true;

    if (stack.piece) {
        stack.layers[stack.count++] = SelectionLayer::Piece;
    }
    if (stack.autonomousUnit) {
        stack.layers[stack.count++] = SelectionLayer::AutonomousUnit;
    }
    if (stack.building) {
        stack.layers[stack.count++] = SelectionLayer::Building;
    }
    if (stack.mapObject) {
        stack.layers[stack.count++] = SelectionLayer::Object;
    }
    stack.layers[stack.count++] = SelectionLayer::Terrain;

    return stack;
}