@CustomDialog
export struct TextInputDialog {
  controller: CustomDialogController;
  initialText: string = '';
  title: string = '';
  prompt: string = '';
  placeholder: string = '';
  confirmText: string = '';
  cancelText: string = '';
  complete: (accepted: boolean, text: string) => void = (_accepted: boolean, _text: string): void => {};
  @State text: string = '';

  aboutToAppear(): void {
    this.text = this.initialText;
  }

  build() {
    Column({ space: 16 }) {
      if (this.title.length > 0) {
        Text(this.title)
          .fontSize(20)
          .fontWeight(FontWeight.Bold);
      }
      if (this.prompt.length > 0) {
        Text(this.prompt)
          .fontSize(16);
      }
      TextInput({ text: this.text, placeholder: this.placeholder })
        .defaultFocus(true)
        .enableKeyboardOnFocus(true)
        .enterKeyType(EnterKeyType.Done)
        .onChange((value: string) => {
          this.text = value;
        })
        .onSubmit((_key: EnterKeyType, event: SubmitEvent) => {
          this.complete(true, event.text);
        });
      Row({ space: 16 }) {
        Button(this.cancelText)
          .layoutWeight(1)
          .onClick(() => this.complete(false, ''));
        Button(this.confirmText)
          .layoutWeight(1)
          .onClick(() => this.complete(true, this.text));
      }
      .width('100%');
    }
    .padding(24)
    .width('100%');
  }
}
