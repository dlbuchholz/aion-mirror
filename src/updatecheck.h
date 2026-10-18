#ifndef UPDATECHECK_H
#define UPDATECHECK_H

class QWidget;

// Entry point for "Check for updates". Fetching and comparing releases isn't implemented
// yet; for now this only tells the user which version they are running.
namespace UpdateCheck {

void checkInteractively(QWidget* parent);

} // namespace UpdateCheck

#endif // UPDATECHECK_H
