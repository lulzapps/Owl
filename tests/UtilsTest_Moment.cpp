// Owl - www.owlclient.com
// Copyright (c) 2012-2019, Adalid Claure <aclaure@gmail.com>

#include <boost/test/unit_test.hpp>

#include <ostream>
#include <QtCore>

#include "../src/Utils/Moment.h"

namespace std
{

std::ostream& operator<<(std::ostream& out, const QString& qstring)
{
    return out << qstring.toStdString();
}

}

BOOST_AUTO_TEST_SUITE(Moment)

BOOST_AUTO_TEST_CASE(MomentTest)
{
    QDateTime fakenow(QDate(2016, 8, 23), QTime(5, 30));

    owl::Moment m;

    m.setDateTime(QDateTime(QDate(2016, 8, 23), QTime(5, 30)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QString("A few seconds ago"));

    m.setDateTime(QDateTime(QDate(2016, 8, 23), QTime(5, 29)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QStringLiteral("A minute ago"));

    m.setDateTime(QDateTime(QDate(2016, 8, 23), QTime(5, 01)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QStringLiteral("29 minutes ago"));

    m.setDateTime(QDateTime(QDate(2016, 8, 23), QTime(4, 30)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QStringLiteral("An hour ago"));

    m.setDateTime(QDateTime(QDate(2016, 8, 23), QTime(2, 45)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QStringLiteral("2 hours ago"));

    m.setDateTime(QDateTime(QDate(2016, 8, 22), QTime(05, 30)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QStringLiteral("a day ago"));

    m.setDateTime(QDateTime(QDate(2016, 8, 13), QTime(5, 29)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QStringLiteral("10 days ago"));

    m.setDateTime(QDateTime(QDate(2016, 7, 23), QTime(15, 29)));
    BOOST_CHECK_EQUAL(m.toString(fakenow), QStringLiteral("2016-07-23"));
}

BOOST_AUTO_TEST_SUITE_END()