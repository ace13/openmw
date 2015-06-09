#include "store.hpp"
#include "esmstore.hpp"
#include "recordcmp.hpp"

#include <stdexcept>
#include <sstream>

#include <components/misc/rng.hpp>

#include <components/esm/esmwriter.hpp>

#include <components/loadinglistener/loadinglistener.hpp>

#include <components/esm/esmreader.hpp>

namespace MWWorld
{
	namespace
	{
		template<typename T>
		class GetRecords {
			const std::string mFind;
			std::vector<const T*> *mRecords;

		public:
			GetRecords(const std::string &str, std::vector<const T*> *records)
				: mFind(Misc::StringUtils::lowerCase(str)), mRecords(records)
			{ }

			void operator()(const T *item)
			{
				if (Misc::StringUtils::ciCompareLen(mFind, item->mId, mFind.size()) == 0)
					mRecords->push_back(item);
			}
		};
	}

	template<class T>
	IndexedStore<T>::IndexedStore() {}

	template<class T>
	typename IndexedStore<T>::iterator IndexedStore<T>::begin() const {
		return mStatic.begin();
	}

	template<class T>
	typename IndexedStore<T>::iterator IndexedStore<T>::end() const {
		return mStatic.end();
	}

	template<class T>
	void IndexedStore<T>::load(ESM::ESMReader &esm) {
		T record;
		record.load(esm);

		// Try to overwrite existing record
		std::pair<typename Static::iterator, bool> ret = mStatic.insert(std::make_pair(record.mIndex, record));
		if (!ret.second)
			ret.first->second = record;
	}

	template<class T>
	int IndexedStore<T>::getSize() const {
		return mStatic.size();
	}

	template<class T>
	void IndexedStore<T>::setUp() {
	}

	template<class T>
	const T* IndexedStore<T>::search(int index) const {
		typename Static::const_iterator it = mStatic.find(index);
		if (it != mStatic.end())
			return &(it->second);
		return NULL;
	}

	template<class T>
	const T* IndexedStore<T>::find(int index) const {
		const T *ptr = search(index);
		if (ptr == 0) {
			std::ostringstream msg;
			msg << "Object with index " << index << " not found";
			throw std::runtime_error(msg.str());
		}
		return ptr;
	}


	template<class T>
	Store<T>::Store()
	{}

	template<class T>
	Store<T>::Store(const Store<T> &orig)
		: mStatic(orig.mStatic)
	{}

	template<class T>
	void Store<T>::clearDynamic()
	{
		// remove the dynamic part of mShared
		assert(mShared.size() >= mStatic.size());
		mShared.erase(mShared.begin() + mStatic.size(), mShared.end());
		mDynamic.clear();
	}

	template<class T>
	const T* Store<T>::search(const std::string &id) const {
		T item;
		item.mId = Misc::StringUtils::lowerCase(id);

		typename Dynamic::const_iterator dit = mDynamic.find(item.mId);
		if (dit != mDynamic.end()) {
			return &dit->second;
		}

		typename std::map<std::string, T>::const_iterator it = mStatic.find(item.mId);

		if (it != mStatic.end() && Misc::StringUtils::ciEqual(it->second.mId, id)) {
			return &(it->second);
		}

		return 0;
	}

	/**
	* Does the record with this ID come from the dynamic store?
	*/
	template<class T>
	bool Store<T>::isDynamic(const std::string &id) const {
		typename Dynamic::const_iterator dit = mDynamic.find(id);
		return (dit != mDynamic.end());
	}

	/** Returns a random record that starts with the named ID, or NULL if not found. */
	template<class T>
	const T* Store<T>::searchRandom(const std::string &id) const
	{
		std::vector<const T*> results;
		std::for_each(mShared.begin(), mShared.end(), GetRecords<T>(id, &results));
		if (!results.empty())
			return results[Misc::Rng::rollDice(results.size())];
		return NULL;
	}

	template<class T>
	const T* Store<T>::find(const std::string &id) const {
		const T *ptr = search(id);
		if (ptr == 0) {
			std::ostringstream msg;
			msg << "Object '" << id << "' not found (const)";
			throw std::runtime_error(msg.str());
		}
		return ptr;
	}

	/** Returns a random record that starts with the named ID. An exception is thrown if none
	* are found. */
	template<class T>
	const T *Store<T>::findRandom(const std::string &id) const
	{
		const T *ptr = searchRandom(id);
		if (ptr == 0)
		{
			std::ostringstream msg;
			msg << "Object starting with '" << id << "' not found (const)";
			throw std::runtime_error(msg.str());
		}
		return ptr;
	}

	template<class T>
	void Store<T>::load(ESM::ESMReader &esm, const std::string &id) {
		std::string idLower = Misc::StringUtils::lowerCase(id);

		std::pair<typename Static::iterator, bool> inserted = mStatic.insert(std::make_pair(idLower, T()));
		if (inserted.second)
			mShared.push_back(&inserted.first->second);

		inserted.first->second.mId = idLower;
		inserted.first->second.load(esm);
	}

	template<class T>
	void Store<T>::setUp() {
	}

	template<class T>
	typename Store<T>::iterator Store<T>::begin() const {
		return mShared.begin();
	}

	template<class T>
	typename Store<T>::iterator Store<T>::end() const {
		return mShared.end();
	}

	template<class T>
	size_t Store<T>::getSize() const {
		return mShared.size();
	}

	template<class T>
	int Store<T>::getDynamicSize() const
	{
		return static_cast<int> (mDynamic.size()); // truncated from unsigned __int64 if _MSC_VER && _WIN64
	}

	template<class T>
	void Store<T>::listIdentifier(std::vector<std::string> &list) const {
		list.reserve(list.size() + getSize());
		typename std::vector<T *>::const_iterator it = mShared.begin();
		for (; it != mShared.end(); ++it) {
			list.push_back((*it)->mId);
		}
	}

	template<class T>
	T* Store<T>::insert(const T &item) {
		std::string id = Misc::StringUtils::lowerCase(item.mId);
		std::pair<typename Dynamic::iterator, bool> result =
			mDynamic.insert(std::pair<std::string, T>(id, item));
		T *ptr = &result.first->second;
		if (result.second) {
			mShared.push_back(ptr);
		}
		else {
			*ptr = item;
		}
		return ptr;
	}

	template<class T>
	T* Store<T>::insertStatic(const T &item) {
		std::string id = Misc::StringUtils::lowerCase(item.mId);
		std::pair<typename Static::iterator, bool> result =
			mStatic.insert(std::pair<std::string, T>(id, item));
		T *ptr = &result.first->second;
		if (result.second) {
			mShared.push_back(ptr);
		}
		else {
			*ptr = item;
		}
		return ptr;
	}


	template<class T>
	bool Store<T>::eraseStatic(const std::string &id) {
		T item;
		item.mId = Misc::StringUtils::lowerCase(id);

		typename std::map<std::string, T>::iterator it = mStatic.find(item.mId);

		if (it != mStatic.end() && Misc::StringUtils::ciEqual(it->second.mId, id)) {
			// delete from the static part of mShared
			typename std::vector<T *>::iterator sharedIter = mShared.begin();
			typename std::vector<T *>::iterator end = sharedIter + mStatic.size();

			while (sharedIter != mShared.end() && sharedIter != end) {
				if ((*sharedIter)->mId == item.mId) {
					mShared.erase(sharedIter);
					break;
				}
				++sharedIter;
			}
			mStatic.erase(it);
		}

		return true;
	}

	template<class T>
	bool Store<T>::erase(const std::string &id) {
		std::string key = Misc::StringUtils::lowerCase(id);
		typename Dynamic::iterator it = mDynamic.find(key);
		if (it == mDynamic.end()) {
			return false;
		}
		mDynamic.erase(it);

		// have to reinit the whole shared part
		assert(mShared.size() >= mStatic.size());
		mShared.erase(mShared.begin() + mStatic.size(), mShared.end());
		for (it = mDynamic.begin(); it != mDynamic.end(); ++it) {
			mShared.push_back(&it->second);
		}
		return true;
	}

	template<class T>
	bool Store<T>::erase(const T &item) {
		return erase(item.mId);
	}

	template<class T>
	void Store<T>::write(ESM::ESMWriter& writer, Loading::Listener& progress) const
	{
		for (typename Dynamic::const_iterator iter(mDynamic.begin()); iter != mDynamic.end();
			++iter)
		{
			writer.startRecord(T::sRecordId);
			writer.writeHNString("NAME", iter->second.mId);
			iter->second.save(writer);
			writer.endRecord(T::sRecordId);
		}
	}

	template<class T>
	void Store<T>::read(ESM::ESMReader& reader, const std::string& id)
	{
		T record;
		record.mId = id;
		record.load(reader);
		insert(record);
	}

	template class IndexedStore<ESM::MagicEffect>;
	template class IndexedStore<ESM::Skill>;

//
// Non-generic
//

namespace
{
	struct LandCompare
	{
		bool operator()(const ESM::Land *x, const ESM::Land *y) {
			if (x->mX == y->mX) {
				return x->mY < y->mY;
			}
			return x->mX < y->mX;
		}
	};
}

// ESM::Attribute

MWWorld::Store<ESM::Attribute>::Store() {
	mStatic.reserve(ESM::Attribute::Length);
}

const ESM::Attribute *MWWorld::Store<ESM::Attribute>::search(size_t index) const {
	if (index >= mStatic.size()) {
		return 0;
	}
	return &mStatic.at(index);
}

const ESM::Attribute *MWWorld::Store<ESM::Attribute>::find(size_t index) const {
	const ESM::Attribute *ptr = search(index);
	if (ptr == 0) {
		std::ostringstream msg;
		msg << "Attribute with index " << index << " not found";
		throw std::runtime_error(msg.str());
	}
	return ptr;
}

void MWWorld::Store<ESM::Attribute>::setUp() {
	for (int i = 0; i < ESM::Attribute::Length; ++i) {
		mStatic.push_back(
			ESM::Attribute(
			ESM::Attribute::sAttributeIds[i],
			ESM::Attribute::sGmstAttributeIds[i],
			ESM::Attribute::sGmstAttributeDescIds[i]
			)
			);
	}
}

size_t MWWorld::Store<ESM::Attribute>::getSize() const {
	return mStatic.size();
}

MWWorld::Store<ESM::Attribute>::iterator MWWorld::Store<ESM::Attribute>::begin() const {
	return mStatic.begin();
}

MWWorld::Store<ESM::Attribute>::iterator MWWorld::Store<ESM::Attribute>::end() const {
	return mStatic.end();
}


// ESM::Cell

const ESM::Cell * MWWorld::Store<ESM::Cell>::search(const ESM::Cell &cell) const {
	if (cell.isExterior()) {
		return search(cell.getGridX(), cell.getGridY());
	}
	return search(cell.mName);
}

void MWWorld::Store<ESM::Cell>::handleMovedCellRefs(ESM::ESMReader& esm, ESM::Cell* cell)
{
	//Handling MovedCellRefs, there is no way to do it inside loadcell
	while (esm.isNextSub("MVRF")) {
		ESM::CellRef ref;
		ESM::MovedCellRef cMRef;
		cell->getNextMVRF(esm, cMRef);

		ESM::Cell *cellAlt = const_cast<ESM::Cell*>(searchOrCreate(cMRef.mTarget[0], cMRef.mTarget[1]));

		// Get regular moved reference data. Adapted from CellStore::loadRefs. Maybe we can optimize the following
		//  implementation when the oher implementation works as well.
		bool deleted = false;
		cell->getNextRef(esm, ref, deleted);

		// Add data required to make reference appear in the correct cell.
		// We should not need to test for duplicates, as this part of the code is pre-cell merge.
		cell->mMovedRefs.push_back(cMRef);
		// But there may be duplicates here!
		if (!deleted)
		{
			ESM::CellRefTracker::iterator iter = std::find(cellAlt->mLeasedRefs.begin(), cellAlt->mLeasedRefs.end(), ref.mRefNum);
			if (iter == cellAlt->mLeasedRefs.end())
				cellAlt->mLeasedRefs.push_back(ref);
			else
				*iter = ref;
		}
	}
}

const ESM::Cell *MWWorld::Store<ESM::Cell>::search(const std::string &id) const
{
	ESM::Cell cell;
	cell.mName = Misc::StringUtils::lowerCase(id);

	std::map<std::string, ESM::Cell>::const_iterator it = mInt.find(cell.mName);

	if (it != mInt.end() && Misc::StringUtils::ciEqual(it->second.mName, id)) {
		return &(it->second);
	}

	DynamicInt::const_iterator dit = mDynamicInt.find(cell.mName);
	if (dit != mDynamicInt.end()) {
		return &dit->second;
	}

	return 0;
}

const ESM::Cell *MWWorld::Store<ESM::Cell>::search(int x, int y) const
{
	ESM::Cell cell;
	cell.mData.mX = x, cell.mData.mY = y;

	std::pair<int, int> key(x, y);
	DynamicExt::const_iterator it = mExt.find(key);
	if (it != mExt.end()) {
		return &(it->second);
	}

	DynamicExt::const_iterator dit = mDynamicExt.find(key);
	if (dit != mDynamicExt.end()) {
		return &dit->second;
	}

	return 0;
}

const ESM::Cell *MWWorld::Store<ESM::Cell>::searchOrCreate(int x, int y)
{
	std::pair<int, int> key(x, y);
	DynamicExt::const_iterator it = mExt.find(key);
	if (it != mExt.end()) {
		return &(it->second);
	}

	DynamicExt::const_iterator dit = mDynamicExt.find(key);
	if (dit != mDynamicExt.end()) {
		return &dit->second;
	}

	ESM::Cell newCell;
	newCell.mData.mX = x;
	newCell.mData.mY = y;
	newCell.mData.mFlags = ESM::Cell::HasWater;
	newCell.mAmbi.mAmbient = 0;
	newCell.mAmbi.mSunlight = 0;
	newCell.mAmbi.mFog = 0;
	newCell.mAmbi.mFogDensity = 0;
	return &mExt.insert(std::make_pair(key, newCell)).first->second;
}

const ESM::Cell *MWWorld::Store<ESM::Cell>::find(const std::string &id) const
{
	const ESM::Cell *ptr = search(id);
	if (ptr == 0) {
		std::ostringstream msg;
		msg << "Interior cell '" << id << "' not found";
		throw std::runtime_error(msg.str());
	}
	return ptr;
}

const ESM::Cell *MWWorld::Store<ESM::Cell>::find(int x, int y) const
{
	const ESM::Cell *ptr = search(x, y);
	if (ptr == 0) {
		std::ostringstream msg;
		msg << "Exterior at (" << x << ", " << y << ") not found";
		throw std::runtime_error(msg.str());
	}
	return ptr;
}

void MWWorld::Store<ESM::Cell>::setUp()
{
	typedef DynamicExt::iterator ExtIterator;
	typedef std::map<std::string, ESM::Cell>::iterator IntIterator;

	mSharedInt.clear();
	mSharedInt.reserve(mInt.size());
	for (IntIterator it = mInt.begin(); it != mInt.end(); ++it) {
		mSharedInt.push_back(&(it->second));
	}

	mSharedExt.clear();
	mSharedExt.reserve(mExt.size());
	for (ExtIterator it = mExt.begin(); it != mExt.end(); ++it) {
		mSharedExt.push_back(&(it->second));
	}
}

void MWWorld::Store<ESM::Cell>::load(ESM::ESMReader &esm, const std::string &id)
{
	// Don't automatically assume that a new cell must be spawned. Multiple plugins write to the same cell,
	//  and we merge all this data into one Cell object. However, we can't simply search for the cell id,
	//  as many exterior cells do not have a name. Instead, we need to search by (x,y) coordinates - and they
	//  are not available until both cells have been loaded at least partially!

	// All cells have a name record, even nameless exterior cells.
	std::string idLower = Misc::StringUtils::lowerCase(id);
	ESM::Cell cell;
	cell.mName = id;

	// Load the (x,y) coordinates of the cell, if it is an exterior cell,
	// so we can find the cell we need to merge with
	cell.loadData(esm);

	if (cell.mData.mFlags & ESM::Cell::Interior)
	{
		// Store interior cell by name, try to merge with existing parent data.
		ESM::Cell *oldcell = const_cast<ESM::Cell*>(search(idLower));
		if (oldcell) {
			// merge new cell into old cell
			// push the new references on the list of references to manage (saveContext = true)
			oldcell->mData = cell.mData;
			oldcell->mName = cell.mName; // merge name just to be sure (ID will be the same, but case could have been changed)
			oldcell->loadCell(esm, true);
		}
		else
		{
			// spawn a new cell
			cell.loadCell(esm, true);

			mInt[idLower] = cell;
		}
	}
	else
	{
		// Store exterior cells by grid position, try to merge with existing parent data.
		ESM::Cell *oldcell = const_cast<ESM::Cell*>(search(cell.getGridX(), cell.getGridY()));
		if (oldcell) {
			// merge new cell into old cell
			oldcell->mData = cell.mData;
			oldcell->mName = cell.mName;
			oldcell->loadCell(esm, false);

			// handle moved ref (MVRF) subrecords
			handleMovedCellRefs(esm, &cell);

			// push the new references on the list of references to manage
			oldcell->postLoad(esm);

			// merge lists of leased references, use newer data in case of conflict
			for (ESM::MovedCellRefTracker::const_iterator it = cell.mMovedRefs.begin(); it != cell.mMovedRefs.end(); ++it) {
				// remove reference from current leased ref tracker and add it to new cell
				ESM::MovedCellRefTracker::iterator itold = std::find(oldcell->mMovedRefs.begin(), oldcell->mMovedRefs.end(), it->mRefNum);
				if (itold != oldcell->mMovedRefs.end()) {
					ESM::MovedCellRef target0 = *itold;
					ESM::Cell *wipecell = const_cast<ESM::Cell*>(search(target0.mTarget[0], target0.mTarget[1]));
					ESM::CellRefTracker::iterator it_lease = std::find(wipecell->mLeasedRefs.begin(), wipecell->mLeasedRefs.end(), it->mRefNum);
					wipecell->mLeasedRefs.erase(it_lease);
					*itold = *it;
				}
				else
					oldcell->mMovedRefs.push_back(*it);
			}

			// We don't need to merge mLeasedRefs of cell / oldcell. This list is filled when another cell moves a
			// reference to this cell, so the list for the new cell should be empty. The list for oldcell,
			// however, could have leased refs in it and so should be kept.
		}
		else
		{
			// spawn a new cell
			cell.loadCell(esm, false);

			// handle moved ref (MVRF) subrecords
			handleMovedCellRefs(esm, &cell);

			// push the new references on the list of references to manage
			cell.postLoad(esm);

			mExt[std::make_pair(cell.mData.mX, cell.mData.mY)] = cell;
		}
	}
}

MWWorld::Store<ESM::Cell>::iterator MWWorld::Store<ESM::Cell>::intBegin() const {
	return iterator(mSharedInt.begin());
}

MWWorld::Store<ESM::Cell>::iterator MWWorld::Store<ESM::Cell>::intEnd() const {
	return iterator(mSharedInt.end());
}

MWWorld::Store<ESM::Cell>::iterator MWWorld::Store<ESM::Cell>::extBegin() const {
	return iterator(mSharedExt.begin());
}

MWWorld::Store<ESM::Cell>::iterator MWWorld::Store<ESM::Cell>::extEnd() const {
	return iterator(mSharedExt.end());
}

// Return the northernmost cell in the easternmost column.
const ESM::Cell *MWWorld::Store<ESM::Cell>::searchExtByName(const std::string &id) const
{
	ESM::Cell *cell = 0;
	std::vector<ESM::Cell *>::const_iterator it = mSharedExt.begin();
	for (; it != mSharedExt.end(); ++it) {
		if (Misc::StringUtils::ciEqual((*it)->mName, id)) {
			if (cell == 0 ||
				((*it)->mData.mX > cell->mData.mX) ||
				((*it)->mData.mX == cell->mData.mX && (*it)->mData.mY > cell->mData.mY))
			{
				cell = *it;
			}
		}
	}
	return cell;
}

// Return the northernmost cell in the easternmost column.
const ESM::Cell *MWWorld::Store<ESM::Cell>::searchExtByRegion(const std::string &id) const
{
	ESM::Cell *cell = 0;
	std::vector<ESM::Cell *>::const_iterator it = mSharedExt.begin();
	for (; it != mSharedExt.end(); ++it) {
		if (Misc::StringUtils::ciEqual((*it)->mRegion, id)) {
			if (cell == 0 ||
				((*it)->mData.mX > cell->mData.mX) ||
				((*it)->mData.mX == cell->mData.mX && (*it)->mData.mY > cell->mData.mY))
			{
				cell = *it;
			}
		}
	}
	return cell;
}

size_t MWWorld::Store<ESM::Cell>::getSize() const {
	return mSharedInt.size() + mSharedExt.size();
}

void MWWorld::Store<ESM::Cell>::listIdentifier(std::vector<std::string> &list) const {
	list.reserve(list.size() + mSharedInt.size());

	std::vector<ESM::Cell *>::const_iterator it = mSharedInt.begin();
	for (; it != mSharedInt.end(); ++it) {
		list.push_back((*it)->mName);
	}
}

ESM::Cell *MWWorld::Store<ESM::Cell>::insert(const ESM::Cell &cell) {
	if (search(cell) != 0) {
		std::ostringstream msg;
		msg << "Failed to create ";
		msg << ((cell.isExterior()) ? "exterior" : "interior");
		msg << " cell";

		throw std::runtime_error(msg.str());
	}
	ESM::Cell *ptr;
	if (cell.isExterior()) {
		std::pair<int, int> key(cell.getGridX(), cell.getGridY());

		// duplicate insertions are avoided by search(ESM::Cell &)
		std::pair<DynamicExt::iterator, bool> result =
			mDynamicExt.insert(std::make_pair(key, cell));

		ptr = &result.first->second;
		mSharedExt.push_back(ptr);
	}
	else {
		std::string key = Misc::StringUtils::lowerCase(cell.mName);

		// duplicate insertions are avoided by search(ESM::Cell &)
		std::pair<DynamicInt::iterator, bool> result =
			mDynamicInt.insert(std::make_pair(key, cell));

		ptr = &result.first->second;
		mSharedInt.push_back(ptr);
	}
	return ptr;
}

bool MWWorld::Store<ESM::Cell>::erase(const ESM::Cell &cell) {
	if (cell.isExterior()) {
		return erase(cell.getGridX(), cell.getGridY());
	}
	return erase(cell.mName);
}

bool MWWorld::Store<ESM::Cell>::erase(const std::string &id) {
	std::string key = Misc::StringUtils::lowerCase(id);
	DynamicInt::iterator it = mDynamicInt.find(key);

	if (it == mDynamicInt.end()) {
		return false;
	}
	mDynamicInt.erase(it);
	mSharedInt.erase(
		mSharedInt.begin() + mSharedInt.size(),
		mSharedInt.end()
		);

	for (it = mDynamicInt.begin(); it != mDynamicInt.end(); ++it) {
		mSharedInt.push_back(&it->second);
	}

	return true;
}

bool MWWorld::Store<ESM::Cell>::erase(int x, int y) {
	std::pair<int, int> key(x, y);
	DynamicExt::iterator it = mDynamicExt.find(key);

	if (it == mDynamicExt.end()) {
		return false;
	}
	mDynamicExt.erase(it);
	mSharedExt.erase(
		mSharedExt.begin() + mSharedExt.size(),
		mSharedExt.end()
		);

	for (it = mDynamicExt.begin(); it != mDynamicExt.end(); ++it) {
		mSharedExt.push_back(&it->second);
	}

	return true;
}


// ESM::Dialogue

template<>
void MWWorld::Store<ESM::Dialogue>::setUp()
{
	// DialInfos marked as deleted are kept during the loading phase, so that the linked list
	// structure is kept intact for inserting further INFOs. Delete them now that loading is done.
	for (Static::iterator it = mStatic.begin(); it != mStatic.end(); ++it)
	{
		ESM::Dialogue& dial = it->second;
		dial.clearDeletedInfos();
	}

	mShared.clear();
	mShared.reserve(mStatic.size());
	std::map<std::string, ESM::Dialogue>::iterator it = mStatic.begin();
	for (; it != mStatic.end(); ++it) {
		mShared.push_back(&(it->second));
	}
}

template<>
void MWWorld::Store<ESM::Dialogue>::load(ESM::ESMReader &esm, const std::string &id)
{
	std::string idLower = Misc::StringUtils::lowerCase(id);

	std::map<std::string, ESM::Dialogue>::iterator it = mStatic.find(idLower);
	if (it == mStatic.end()) {
		it = mStatic.insert(std::make_pair(idLower, ESM::Dialogue())).first;
		it->second.mId = id; // don't smash case here, as this line is printed
	}

	it->second.load(esm);
}


// ESM::Land

MWWorld::Store<ESM::Land>::~Store()
{
	for (std::vector<ESM::Land *>::const_iterator it =
		mStatic.begin(); it != mStatic.end(); ++it)
	{
		delete *it;
	}
}

size_t MWWorld::Store<ESM::Land>::getSize() const {
	return mStatic.size();
}

MWWorld::Store<ESM::Land>::iterator MWWorld::Store<ESM::Land>::begin() const {
	return iterator(mStatic.begin());
}

MWWorld::Store<ESM::Land>::iterator MWWorld::Store<ESM::Land>::end() const {
	return iterator(mStatic.end());
}


// Must be threadsafe! Called from terrain background loading threads.
// Not a big deal here, since ESM::Land can never be modified or inserted/erased
ESM::Land *MWWorld::Store<ESM::Land>::search(int x, int y) const {
	ESM::Land land;
	land.mX = x, land.mY = y;

	std::vector<ESM::Land *>::const_iterator it =
		std::lower_bound(mStatic.begin(), mStatic.end(), &land, LandCompare());

	if (it != mStatic.end() && (*it)->mX == x && (*it)->mY == y) {
		return const_cast<ESM::Land *>(*it);
	}
	return 0;
}

ESM::Land *MWWorld::Store<ESM::Land>::find(int x, int y) const{
	ESM::Land *ptr = search(x, y);
	if (ptr == 0) {
		std::ostringstream msg;
		msg << "Land at (" << x << ", " << y << ") not found";
		throw std::runtime_error(msg.str());
	}
	return ptr;
}

void MWWorld::Store<ESM::Land>::load(ESM::ESMReader &esm, const std::string &id) {
	ESM::Land *ptr = new ESM::Land();
	ptr->load(esm);

	// Same area defined in multiple plugins? -> last plugin wins
	// Can't use search() because we aren't sorted yet - is there any other way to speed this up?
	for (std::vector<ESM::Land*>::iterator it = mStatic.begin(); it != mStatic.end(); ++it)
	{
		if ((*it)->mX == ptr->mX && (*it)->mY == ptr->mY)
		{
			delete *it;
			mStatic.erase(it);
			break;
		}
	}

	mStatic.push_back(ptr);
}

void MWWorld::Store<ESM::Land>::setUp() {
	std::sort(mStatic.begin(), mStatic.end(), LandCompare());
}


// ESM::LandTexture

MWWorld::Store<ESM::LandTexture>::Store()
{
	mStatic.push_back(LandTextureList());
	LandTextureList &ltexl = mStatic[0];
	// More than enough to hold Morrowind.esm. Extra lists for plugins will we
	//  added on-the-fly in a different method.
	ltexl.reserve(128);
}

// Must be threadsafe! Called from terrain background loading threads.
// Not a big deal here, since ESM::LandTexture can never be modified or inserted/erased
const ESM::LandTexture * MWWorld::Store<ESM::LandTexture>::search(size_t index, size_t plugin) const {
	assert(plugin < mStatic.size());
	const LandTextureList &ltexl = mStatic[plugin];

	assert(index < ltexl.size());
	return &ltexl.at(index);
}

const ESM::LandTexture * MWWorld::Store<ESM::LandTexture>::find(size_t index, size_t plugin) const {
	const ESM::LandTexture *ptr = search(index, plugin);
	if (ptr == 0) {
		std::ostringstream msg;
		msg << "Land texture with index " << index << " not found";
		throw std::runtime_error(msg.str());
	}
	return ptr;
}

size_t MWWorld::Store<ESM::LandTexture>::getSize() const {
	return mStatic.size();
}

size_t MWWorld::Store<ESM::LandTexture>::getSize(size_t plugin) const {
	assert(plugin < mStatic.size());
	return mStatic[plugin].size();
}

void MWWorld::Store<ESM::LandTexture>::load(ESM::ESMReader &esm, const std::string &id, size_t plugin)
{
	ESM::LandTexture lt;
	lt.load(esm);
	lt.mId = id;

	// Make sure we have room for the structure
	if (plugin >= mStatic.size()) {
		mStatic.resize(plugin + 1);
	}
	LandTextureList &ltexl = mStatic[plugin];
	if (lt.mIndex + 1 > (int)ltexl.size())
		ltexl.resize(lt.mIndex + 1);

	// Store it
	ltexl[lt.mIndex] = lt;
}

MWWorld::Store<ESM::LandTexture>::iterator MWWorld::Store<ESM::LandTexture>::begin(size_t plugin) const {
	assert(plugin < mStatic.size());
	return mStatic[plugin].begin();
}

MWWorld::Store<ESM::LandTexture>::iterator MWWorld::Store<ESM::LandTexture>::end(size_t plugin) const {
	assert(plugin < mStatic.size());
	return mStatic[plugin].end();
}

void MWWorld::Store<ESM::LandTexture>::load(ESM::ESMReader &esm, const std::string &id)
{
    load(esm, id, esm.getIndex());
}


// ESM::Pathgrid

MWWorld::Store<ESM::Pathgrid>::Store()
	: mCells(NULL)
{
}

void MWWorld::Store<ESM::Pathgrid>::setCells(Store<ESM::Cell>& cells)
{
	mCells = &cells;
}

void MWWorld::Store<ESM::Pathgrid>::load(ESM::ESMReader &esm, const std::string &id) {
	ESM::Pathgrid pathgrid;
	pathgrid.load(esm);

	// Unfortunately the Pathgrid record model does not specify whether the pathgrid belongs to an interior or exterior cell.
	// For interior cells, mCell is the cell name, but for exterior cells it is either the cell name or if that doesn't exist, the cell's region name.
	// mX and mY will be (0,0) for interior cells, but there is also an exterior cell with the coordinates of (0,0), so that doesn't help.
	// Check whether mCell is an interior cell. This isn't perfect, will break if a Region with the same name as an interior cell is created.
	// A proper fix should be made for future versions of the file format.
	bool interior = mCells->search(pathgrid.mCell) != NULL;

	// Try to overwrite existing record
	if (interior)
	{
		std::pair<Interior::iterator, bool> ret = mInt.insert(std::make_pair(pathgrid.mCell, pathgrid));
		if (!ret.second)
			ret.first->second = pathgrid;
	}
	else
	{
		std::pair<Exterior::iterator, bool> ret = mExt.insert(std::make_pair(std::make_pair(pathgrid.mData.mX, pathgrid.mData.mY), pathgrid));
		if (!ret.second)
			ret.first->second = pathgrid;
	}
}

size_t MWWorld::Store<ESM::Pathgrid>::getSize() const {
	return mInt.size() + mExt.size();
}

void MWWorld::Store<ESM::Pathgrid>::setUp() {
}

const ESM::Pathgrid *MWWorld::Store<ESM::Pathgrid>::search(int x, int y) const {
	Exterior::const_iterator it = mExt.find(std::make_pair(x, y));
	if (it != mExt.end())
		return &(it->second);
	return NULL;
}

const ESM::Pathgrid *MWWorld::Store<ESM::Pathgrid>::search(const std::string& name) const {
	Interior::const_iterator it = mInt.find(name);
	if (it != mInt.end())
		return &(it->second);
	return NULL;
}

const ESM::Pathgrid *MWWorld::Store<ESM::Pathgrid>::find(int x, int y) const {
	const ESM::Pathgrid* pathgrid = search(x, y);
	if (!pathgrid)
	{
		std::ostringstream msg;
		msg << "Pathgrid in cell '" << x << " " << y << "' not found";
		throw std::runtime_error(msg.str());
	}
	return pathgrid;
}

const ESM::Pathgrid* MWWorld::Store<ESM::Pathgrid>::find(const std::string& name) const {
	const ESM::Pathgrid* pathgrid = search(name);
	if (!pathgrid)
	{
		std::ostringstream msg;
		msg << "Pathgrid in cell '" << name << "' not found";
		throw std::runtime_error(msg.str());
	}
	return pathgrid;
}

const ESM::Pathgrid *MWWorld::Store<ESM::Pathgrid>::search(const ESM::Cell &cell) const {
	if (!(cell.mData.mFlags & ESM::Cell::Interior))
		return search(cell.mData.mX, cell.mData.mY);
	else
		return search(cell.mName);
}

const ESM::Pathgrid *MWWorld::Store<ESM::Pathgrid>::find(const ESM::Cell &cell) const {
	if (!(cell.mData.mFlags & ESM::Cell::Interior))
		return find(cell.mData.mX, cell.mData.mY);
	else
		return find(cell.mName);
}


// ESM::Script

template<>
void MWWorld::Store<ESM::Script>::load(ESM::ESMReader &esm, const std::string &id)
{
	ESM::Script scpt;
	scpt.load(esm);
	Misc::StringUtils::toLower(scpt.mId);

	std::pair<typename Static::iterator, bool> inserted = mStatic.insert(std::make_pair(scpt.mId, scpt));
	if (inserted.second)
		mShared.push_back(&inserted.first->second);
	else
		inserted.first->second = scpt;
}


// ESM::StartScript
template<>
void MWWorld::Store<ESM::StartScript>::load(ESM::ESMReader &esm, const std::string &id)
{
	ESM::StartScript s;
	s.load(esm);
	s.mId = Misc::StringUtils::toLower(s.mId);
	std::pair<typename Static::iterator, bool> inserted = mStatic.insert(std::make_pair(s.mId, s));
	if (inserted.second)
		mShared.push_back(&inserted.first->second);
	else
		inserted.first->second = s;
}

	// Explicit instantiations

	template class Store<ESM::Activator>;
	template class Store<ESM::Apparatus>;
	template class Store<ESM::Armor>;
	template class Store<ESM::Attribute>;
	template class Store<ESM::BirthSign>;
	template class Store<ESM::BodyPart>;
	template class Store<ESM::Book>;
	template class Store<ESM::Cell>;
	template class Store<ESM::Class>;
	template class Store<ESM::Clothing>;
	template class Store<ESM::Container>;
	template class Store<ESM::Creature>;
	template class Store<ESM::CreatureLevList>;
	template class Store<ESM::Dialogue>;
	template class Store<ESM::Door>;
	template class Store<ESM::Enchantment>;
	template class Store<ESM::Faction>;
	template class Store<ESM::GameSetting>;
	template class Store<ESM::Global>;
	template class Store<ESM::Ingredient>;
	template class Store<ESM::ItemLevList>;
	template class Store<ESM::Land>;
	template class Store<ESM::LandTexture>;
	template class Store<ESM::Light>;
	template class Store<ESM::Lockpick>;
	template class Store<ESM::MagicEffect>;
	template class Store<ESM::Miscellaneous>;
	template class Store<ESM::NPC>;
	template class Store<ESM::Pathgrid>;
	template class Store<ESM::Potion>;
	template class Store<ESM::Probe>;
	template class Store<ESM::Race>;
	template class Store<ESM::Region>;
	template class Store<ESM::Repair>;
	template class Store<ESM::Script>;
	template class Store<ESM::Skill>;
	template class Store<ESM::Sound>;
	template class Store<ESM::SoundGenerator>;
	template class Store<ESM::Spell>;
	template class Store<ESM::StartScript>;
	template class Store<ESM::Static>;
	template class Store<ESM::Weapon>;
}
