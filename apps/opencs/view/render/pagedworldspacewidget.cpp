
#include "pagedworldspacewidget.hpp"

#include <sstream>

#include <QMouseEvent>

#include <osgGA/TrackballManipulator>

#include <components/esm/loadland.hpp>

#include "../../model/world/tablemimedata.hpp"
#include "../../model/world/idtable.hpp"

#include "../widget/scenetooltoggle.hpp"
#include "../widget/scenetoolmode.hpp"
#include "../widget/scenetooltoggle2.hpp"

#include "editmode.hpp"
#include "elements.hpp"

bool CSVRender::PagedWorldspaceWidget::adjustCells()
{
    bool modified = false;

    const CSMWorld::IdCollection<CSMWorld::Cell>& cells = mDocument.getData().getCells();

    {
        // remove (or name/region modified)
        std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());

        while (iter!=mCells.end())
        {
            int index = cells.searchId (iter->first.getId (mWorldspace));

            if (!mSelection.has (iter->first) || index==-1 ||
                cells.getRecord (index).mState==CSMWorld::RecordBase::State_Deleted)
            {
                delete iter->second;
                mCells.erase (iter++);

                modified = true;
            }
            else
            {
                // check if name or region field has changed
                // FIXME: config setting
                //std::string name = cells.getRecord(index).get().mName;
                //std::string region = cells.getRecord(index).get().mRegion;

                // cell marker update goes here

                ++iter;
            }
        }
    }

    // add
    for (CSMWorld::CellSelection::Iterator iter (mSelection.begin()); iter!=mSelection.end();
        ++iter)
    {
        int index = cells.searchId (iter->getId (mWorldspace));

        if (index > 0 && cells.getRecord (index).mState!=CSMWorld::RecordBase::State_Deleted &&
            mCells.find (*iter)==mCells.end())
        {
            Cell *cell = new Cell (mDocument.getData(), mRootNode,
                    iter->getId (mWorldspace));
            mCells.insert (std::make_pair (*iter, cell));

            modified = true;
        }
    }

    if (modified)
        mView->setCameraManipulator(new osgGA::TrackballManipulator);

    return modified;
}

void CSVRender::PagedWorldspaceWidget::addVisibilitySelectorButtons (
    CSVWidget::SceneToolToggle2 *tool)
{
    WorldspaceWidget::addVisibilitySelectorButtons (tool);
    tool->addButton (Element_Terrain, "Terrain");
    tool->addButton (Element_Fog, "Fog", "", true);
}

void CSVRender::PagedWorldspaceWidget::addEditModeSelectorButtons (
    CSVWidget::SceneToolMode *tool)
{
    WorldspaceWidget::addEditModeSelectorButtons (tool);

    /// \todo replace EditMode with suitable subclasses
    tool->addButton (
        new EditMode (this, QIcon (":placeholder"), Element_Reference, "Terrain shape editing"),
        "terrain-shape");
    tool->addButton (
        new EditMode (this, QIcon (":placeholder"), Element_Reference, "Terrain texture editing"),
        "terrain-texture");
    tool->addButton (
        new EditMode (this, QIcon (":placeholder"), Element_Reference, "Terrain vertex paint editing"),
        "terrain-vertex");
    tool->addButton (
        new EditMode (this, QIcon (":placeholder"), Element_Reference, "Terrain movement"),
        "terrain-move");
}

void CSVRender::PagedWorldspaceWidget::referenceableDataChanged (const QModelIndex& topLeft,
    const QModelIndex& bottomRight)
{
    for (std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());
        iter!=mCells.end(); ++iter)
        if (iter->second->referenceableDataChanged (topLeft, bottomRight))
            flagAsModified();
}

void CSVRender::PagedWorldspaceWidget::referenceableAboutToBeRemoved (
    const QModelIndex& parent, int start, int end)
{
    for (std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());
        iter!=mCells.end(); ++iter)
        if (iter->second->referenceableAboutToBeRemoved (parent, start, end))
            flagAsModified();
}

void CSVRender::PagedWorldspaceWidget::referenceableAdded (const QModelIndex& parent,
    int start, int end)
{
    CSMWorld::IdTable& referenceables = dynamic_cast<CSMWorld::IdTable&> (
        *mDocument.getData().getTableModel (CSMWorld::UniversalId::Type_Referenceables));

    for (std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());
        iter!=mCells.end(); ++iter)
    {
        QModelIndex topLeft = referenceables.index (start, 0);
        QModelIndex bottomRight =
            referenceables.index (end, referenceables.columnCount());

        if (iter->second->referenceableDataChanged (topLeft, bottomRight))
            flagAsModified();
    }
}

void CSVRender::PagedWorldspaceWidget::referenceDataChanged (const QModelIndex& topLeft,
    const QModelIndex& bottomRight)
{
    for (std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());
        iter!=mCells.end(); ++iter)
        if (iter->second->referenceDataChanged (topLeft, bottomRight))
            flagAsModified();
}

void CSVRender::PagedWorldspaceWidget::referenceAboutToBeRemoved (const QModelIndex& parent,
    int start, int end)
{
    for (std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());
        iter!=mCells.end(); ++iter)
        if (iter->second->referenceAboutToBeRemoved (parent, start, end))
            flagAsModified();
}

void CSVRender::PagedWorldspaceWidget::referenceAdded (const QModelIndex& parent, int start,
    int end)
{
    for (std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());
        iter!=mCells.end(); ++iter)
        if (iter->second->referenceAdded (parent, start, end))
            flagAsModified();
}

std::string CSVRender::PagedWorldspaceWidget::getStartupInstruction()
{
    osg::Vec3d position = mView->getCamera()->getViewMatrix().getTrans();
    std::ostringstream stream;

    stream
        << "player->position "
        << position.x() << ", " << position.y() << ", " << position.z()
        << ", 0";

    return stream.str();
}

CSVRender::PagedWorldspaceWidget::PagedWorldspaceWidget (QWidget* parent, CSMDoc::Document& document)
: WorldspaceWidget (document, parent), mDocument (document), mWorldspace ("std::default"),
  mControlElements(NULL), mDisplayCellCoord(true)
{
    QAbstractItemModel *cells =
        document.getData().getTableModel (CSMWorld::UniversalId::Type_Cells);

    connect (cells, SIGNAL (dataChanged (const QModelIndex&, const QModelIndex&)),
        this, SLOT (cellDataChanged (const QModelIndex&, const QModelIndex&)));
    connect (cells, SIGNAL (rowsRemoved (const QModelIndex&, int, int)),
        this, SLOT (cellRemoved (const QModelIndex&, int, int)));
    connect (cells, SIGNAL (rowsInserted (const QModelIndex&, int, int)),
        this, SLOT (cellAdded (const QModelIndex&, int, int)));
}

CSVRender::PagedWorldspaceWidget::~PagedWorldspaceWidget()
{
    for (std::map<CSMWorld::CellCoordinates, Cell *>::iterator iter (mCells.begin());
        iter!=mCells.end(); ++iter)
    {
        delete iter->second;
    }
}

void CSVRender::PagedWorldspaceWidget::useViewHint (const std::string& hint)
{
    if (!hint.empty())
    {
        CSMWorld::CellSelection selection;

        if (hint[0]=='c')
        {
            // syntax: c:#x1 y1; #x2 y2 (number of coordinate pairs can be 0 or larger)
            char ignore;

            std::istringstream stream (hint.c_str());
            if (stream >> ignore)
            {
                char ignore1; // : or ;
                char ignore2; // #
                int x, y;

                while (stream >> ignore1 >> ignore2 >> x >> y)
                    selection.add (CSMWorld::CellCoordinates (x, y));

                /// \todo adjust camera position
            }
        }
        else if (hint[0]=='r')
        {
            /// \todo implement 'r' type hints
        }

        setCellSelection (selection);
    }
}

void CSVRender::PagedWorldspaceWidget::setCellSelection (const CSMWorld::CellSelection& selection)
{
    mSelection = selection;

    if (adjustCells())
        flagAsModified();

    emit cellSelectionChanged (mSelection);
}

std::pair< int, int > CSVRender::PagedWorldspaceWidget::getCoordinatesFromId (const std::string& record) const
{
    std::istringstream stream (record.c_str());
    char ignore;
    int x, y;
    stream >> ignore >> x >> y;
    return std::make_pair(x, y);
}

bool CSVRender::PagedWorldspaceWidget::handleDrop (
    const std::vector< CSMWorld::UniversalId >& data, DropType type)
{
    if (WorldspaceWidget::handleDrop (data, type))
        return true;

    if (type!=Type_CellsExterior)
        return false;

    bool selectionChanged = false;
    for (unsigned i = 0; i < data.size(); ++i)
    {
        std::pair<int, int> coordinates(getCoordinatesFromId(data[i].getId()));
        if (mSelection.add(CSMWorld::CellCoordinates(coordinates.first, coordinates.second)))
        {
            selectionChanged = true;
        }
    }
    if (selectionChanged)
    {
        if (adjustCells())
            flagAsModified();

        emit cellSelectionChanged(mSelection);
    }

    return true;
}

CSVRender::WorldspaceWidget::dropRequirments CSVRender::PagedWorldspaceWidget::getDropRequirements (CSVRender::WorldspaceWidget::DropType type) const
{
    dropRequirments requirements = WorldspaceWidget::getDropRequirements (type);

    if (requirements!=ignored)
        return requirements;

    switch (type)
    {
        case Type_CellsExterior:
            return canHandle;

        case Type_CellsInterior:
            return needUnpaged;

        default:
            return ignored;
    }
}

unsigned int CSVRender::PagedWorldspaceWidget::getVisibilityMask() const
{
    return WorldspaceWidget::getVisibilityMask() | mControlElements->getSelection();
}

CSVWidget::SceneToolToggle *CSVRender::PagedWorldspaceWidget::makeControlVisibilitySelector (
    CSVWidget::SceneToolbar *parent)
{
    mControlElements = new CSVWidget::SceneToolToggle (parent,
        "Controls & Guides Visibility", ":placeholder");

    mControlElements->addButton (":placeholder", Element_CellMarker, ":placeholder",
        "Cell marker");
    mControlElements->addButton (":placeholder", Element_CellArrow, ":placeholder", "Cell arrows");
    mControlElements->addButton (":placeholder", Element_CellBorder, ":placeholder", "Cell border");

    mControlElements->setSelection (0xffffffff);

    connect (mControlElements, SIGNAL (selectionChanged()),
        this, SLOT (elementSelectionChanged()));

    return mControlElements;
}

void CSVRender::PagedWorldspaceWidget::cellDataChanged (const QModelIndex& topLeft,
    const QModelIndex& bottomRight)
{
    /// \todo check if no selected cell is affected and do not update, if that is the case
    if (adjustCells())
        flagAsModified();
}

void CSVRender::PagedWorldspaceWidget::cellRemoved (const QModelIndex& parent, int start,
    int end)
{
    if (adjustCells())
        flagAsModified();
}

void CSVRender::PagedWorldspaceWidget::cellAdded (const QModelIndex& index, int start,
    int end)
{
    /// \todo check if no selected cell is affected and do not update, if that is the case
    if (adjustCells())
        flagAsModified();
}
