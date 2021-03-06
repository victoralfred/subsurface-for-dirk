// SPDX-License-Identifier: GPL-2.0
#include "qt-models/divepicturemodel.h"
#include "core/dive.h"
#include "core/metrics.h"
#include "core/divelist.h"
#include "core/imagedownloader.h"

#include <QtConcurrent>

extern QHash <QString, QImage > thumbnailCache;


SPixmap scaleImages(picturepointer picture)
{
	SPixmap ret;
	ret.first = picture;
	if (thumbnailCache.contains(picture->filename) && !thumbnailCache.value(picture->filename).isNull()) {
		ret.second = thumbnailCache.value(picture->filename);
	} else {
		int dim = defaultIconMetrics().sz_pic;
		QImage p = SHashedImage(picture);
		if(!p.isNull()) {
			p = p.scaled(dim, dim, Qt::KeepAspectRatio);
			thumbnailCache.insert(picture->filename, p);
		}
		ret.second = p;
	}
	return ret;
}


DivePictureModel *DivePictureModel::instance()
{
	static DivePictureModel *self = new DivePictureModel();
	return self;
}

DivePictureModel::DivePictureModel() : numberOfPictures(0)
{
}


void DivePictureModel::updateDivePicturesWhenDone(QList<QFuture<void> > futures)
{
	Q_FOREACH (QFuture<void> f, futures) {
		f.waitForFinished();
	}
	updateDivePictures();
}

void DivePictureModel::updateDivePictures()
{
	if (numberOfPictures != 0) {
		beginRemoveRows(QModelIndex(), 0, numberOfPictures - 1);
		numberOfPictures = 0;
		endRemoveRows();
	}

	// if the dive_table is empty, ignore the displayed_dive
	numberOfPictures = dive_table.nr == 0 ? 0 : dive_get_picture_count(&displayed_dive);
	if (numberOfPictures == 0) {
		return;
	}

	stringPixmapCache.clear();
	SPictureList pictures;
	FOR_EACH_PICTURE_NON_PTR(displayed_dive) {
		stringPixmapCache[QString(picture->filename)].offsetSeconds = picture->offset.seconds;
		pictures.push_back(picture);
	}

	QList<SPixmap> list = QtConcurrent::blockingMapped(pictures, scaleImages);
	Q_FOREACH (const SPixmap &pixmap, list)
		stringPixmapCache[pixmap.first->filename].image = pixmap.second;

	beginInsertRows(QModelIndex(), 0, numberOfPictures - 1);
	endInsertRows();
}

int DivePictureModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 2;
}

QVariant DivePictureModel::data(const QModelIndex &index, int role) const
{
	QVariant ret;
	if (!index.isValid())
		return ret;

	QString key = stringPixmapCache.keys().at(index.row());
	if (index.column() == 0) {
		switch (role) {
		case Qt::ToolTipRole:
			ret = key;
			break;
		case Qt::DecorationRole:
			ret = stringPixmapCache[key].image;
			break;
		case Qt::DisplayRole:
			ret = QFileInfo(key).fileName();
			break;
		case Qt::DisplayPropertyRole:
			ret = QFileInfo(key).filePath();
		}
	} else if (index.column() == 1) {
		switch (role) {
		case Qt::UserRole:
			ret = QVariant::fromValue((int)stringPixmapCache[key].offsetSeconds);
		break;
		case Qt::DisplayRole:
			ret = key;
		}
	}
	return ret;
}

void DivePictureModel::removePicture(const QString &fileUrl, bool last)
{
	dive_remove_picture(fileUrl.toUtf8().data());
	if (last) {
		copy_dive(current_dive, &displayed_dive);
		updateDivePictures();
		mark_divelist_changed(true);
	}
}

int DivePictureModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return numberOfPictures;
}
