/*
 * Button representing user's Avatar
 *
 * Copyright (C) 2011  Martin Klapetek <martin.klapetek@gmail.com>
 * Copyright (C) 2011, 2012 David Edmundson <kde@davidedmundson.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SELECTIMAGEBUTTON_H
#define SELECTIMAGEBUTTON_H

#include <QToolButton>

class SelectImageButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(QString imagePath READ imagePath WRITE setImagePath NOTIFY imagePathChanged USER true)
public:
    SelectImageButton(QWidget* parent = 0);
    virtual ~SelectImageButton();

    //we use QString rather that QUrl because it seems to work better with KConfigXT
    void setImagePath(const QString &imagePath);
    QString imagePath() const;

Q_SIGNALS:
    void imagePathChanged(QString);

private Q_SLOTS:
    void onLoadImageFromFile();
    void onClearImage();

private:
    QString m_imagePath;
};

#endif  //AVATAR_BUTTON_H
