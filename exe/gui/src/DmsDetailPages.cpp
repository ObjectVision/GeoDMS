#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QTextBrowser>

#include "DmsEventLog.h"
#include "DmsMainWindow.h"
#include <QMainWindow>

auto createDetailPages(MainWindow* dms_main_window) -> QPointer<QTextBrowser>
{
    auto dock = new QDockWidget(QObject::tr("DetailPages"), dms_main_window);
    QPointer<QTextBrowser> detail_pages_textbrowser_pointer = new QTextBrowser(dock);
    detail_pages_textbrowser_pointer->setMarkdown("# Fisher's Natural Breaks Classification complexity proof\n"
        "An ***O(k x n x log(n))*** algorithm is presented here, with proof of its validity and complexity, for the [[classification]] of an array of* n* numeric\n"
        "values into * k * classes such that the sum of the squared deviations from the class means is minimal, known as Fisher's Natural Breaks Classification. This algorithm is an improvement of [Jenks' Natural Breaks Classification Method](http://en.wikipedia.org/wiki/Jenks_natural_breaks_optimization),\n"
        "which is a(re)implementation of the algorithm described by Fisher within the context of [choropleth maps](http://en.wikipedia.org/wiki/Choropleth_map), which has time complexity ***O(k x n <sup>2</sup>)***.\n"
        "\n"
        "Finding breaks for 15 classes for a data array of 7 million unique values now takes 20 seconds on a desktop PC.\n"
        "\n"
        "## definitions\n"
        "Given a sequence of strictly increasing values ***v<sub>i</sub>*** and positive weights ***w<sub>i</sub>*** for ***i ? {1..n}***, and a given number of classes ***k*** with ***k = n***,\n"
        "\n"
        "choose classification function ***I(x)*** : {1..***n***} ? {1..***k***}, such that ***SSD<sub>n, k***</sub>, the sum of the squares of the deviations from the class means, is minimal, where :\n"
        "\n"
        //"$$ \\begin{align} SSD_{n,k} &: = \\min\\limits_{I: \\{1..n\\} \\to \\{1..k\\}} \\sum\\limits ^ k_{j = 1} ssd(S_j) & &\\text{minimal sum of sum of squared deviations} \\\\ S_j & : = \\{i\\in \\{1..n\\}\ | I(i) = j\\}&& \\text{set of indices that map to class}\\, j\\\\ ssd(S)& : = {\\sum\\limits_{i \\in S} {w_i(v_i - m(S)) ^ 2}} && \\text{the sum of squared deviations of the values of any index set}\\, S\\\\ m(S)& : = {s(S) \\over w(S)} && \\text{the weighted mean of the values of any index set}\\, S\\\\ s(S)& : = {\\sum\\limits_{i \\in S} {w_i v_i}} && \\text{the weighted sum of the values of any index set}\\, S\\\\ w(S)& : = \\sum\\limits_{i \\in S} {w_i}&& \\text{the sum of the value - weights of any index set}\\, S \\end{align} $$\n"
        //"\n"
        "Note that any array of n unsorted and not necessarily unique values can be sorted and made into unique*** v<sub>i*** < / sub> by removing duplicates with * **w<sub>i * **< / sub> representing the occurrence frequency of each value in * **O(nlog(n)) * **time.);\n");

    detail_pages_textbrowser_pointer->setOpenExternalLinks(true);
    dock->setWidget(detail_pages_textbrowser_pointer);
    dock->setTitleBarWidget(new QWidget(dock));
    //dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dms_main_window->addDockWidget(Qt::RightDockWidgetArea, dock);
    
    return detail_pages_textbrowser_pointer;
    //viewMenu->addAction(dock->toggleViewAction());
}